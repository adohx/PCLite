//
// Created by cj on 2026-06-14.
//

#include "indexer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "attribute_handler/attribute_handler.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "concurrent_writer.h"
#include "octree_naming.h"

static constexpr int kMaxLocalDepth = 10;

// ── Constructor ───────────────────────────────────────────────────────────────

Indexer::Indexer(Attributes attributes, std::string targetDir,
                  std::shared_ptr<ConcurrentWriter> writer,
                  std::unique_ptr<Sampler> sampler,
                  ConverterOptions options, double rootSpacing)
    : attributes_(std::move(attributes)),
      targetDir_(std::move(targetDir)),
      writer_(std::move(writer)),
      sampler_(std::move(sampler)),
      options_(options),
      rootSpacing_(rootSpacing) {
    rowStride_ = attributes_.getTotalBytes();
    posOffset_  = attributes_.getOffset("position");
    for (const Attribute &a : attributes_) {
        if (a.name_ == "position") { posAttr_ = a; break; }
    }
    posHandler_ = AttributeHandlerRegistry::get(posAttr_);
}

// ── Morton sort ───────────────────────────────────────────────────────────────

void Indexer::mortonSort(const std::vector<uint8_t> &rows, const BoundingBoxd &bb,
                          std::vector<uint32_t> &sortedIdx,
                          std::vector<uint64_t> &codes) const {
    if (rowStride_ == 0 || rows.empty()) return;
    uint64_t n = rows.size() / rowStride_;

    codes.resize(n);
    sortedIdx.resize(n);
    std::iota(sortedIdx.begin(), sortedIdx.end(), 0u);

    for (uint64_t i = 0; i < n; ++i) {
        double pos[3] = {};
        posHandler_->decode(rows.data() + i * rowStride_ + posOffset_, posAttr_, pos);
        codes[i] = octree_naming::mortonOf({pos[0], pos[1], pos[2]}, bb);
    }

    std::sort(sortedIdx.begin(), sortedIdx.end(),
              [&](uint32_t a, uint32_t b) { return codes[a] < codes[b]; });
}

// ── buildLocalOctree ──────────────────────────────────────────────────────────

void Indexer::buildLocalOctree(const std::shared_ptr<Node> &node,
                                const std::vector<uint8_t> &rows,
                                const std::vector<uint32_t> &sortedIdx,
                                const std::vector<uint64_t> &codes,
                                size_t begin, size_t end,
                                int level, int maxDepth,
                                bool allowRefinement,
                                std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const {
    size_t n = end - begin;
    if (n == 0) return;

    bool atLeaf = (level >= maxDepth) || (n <= static_cast<size_t>(options_.firstChunkSize));

    if (atLeaf) {
        if (allowRefinement && n > static_cast<size_t>(options_.firstChunkSize) && level >= maxDepth) {
            // Refinement: copy this sub-range, re-sort relative to node->bb_,
            // and recurse from level 0.
            std::vector<uint8_t> subRows(n * rowStride_);
            for (size_t i = 0; i < n; ++i)
                std::memcpy(subRows.data() + i * rowStride_,
                            rows.data() + sortedIdx[begin + i] * rowStride_,
                            rowStride_);

            std::vector<uint32_t> subIdx;
            std::vector<uint64_t> subCodes;
            mortonSort(subRows, node->bb_, subIdx, subCodes);
            buildLocalOctree(node, subRows, subIdx, subCodes,
                             0, n, 0, maxDepth, false, nodePoints);
        } else {
            std::vector<uint8_t> &pts = nodePoints[node.get()];
            pts.resize(n * rowStride_);
            for (size_t i = 0; i < n; ++i)
                std::memcpy(pts.data() + i * rowStride_,
                            rows.data() + sortedIdx[begin + i] * rowStride_,
                            rowStride_);
        }
        return;
    }

    // Scan sorted range for per-octant boundaries.
    size_t childBegin[9];
    childBegin[0] = begin;
    for (int c = 0; c < 8; ++c) {
        size_t i = childBegin[c];
        while (i < end && octree_naming::mortonChildAt(codes[sortedIdx[i]], level) == c) ++i;
        childBegin[c + 1] = i;
    }

    for (int c = 0; c < 8; ++c) {
        if (childBegin[c] == childBegin[c + 1]) continue;
        BoundingBoxd childBB = octree_naming::childBoundingBox(node->bb_, c);
        auto child = std::make_shared<Node>(node->name_ + std::to_string(c), childBB, node);
        child->level_ = node->level_ + 1;
        maxLevel_ = std::max(maxLevel_, child->level_);
        node->children_[c] = child;
        buildLocalOctree(child, rows, sortedIdx, codes,
                         childBegin[c], childBegin[c + 1],
                         level + 1, maxDepth, allowRefinement, nodePoints);
    }
}

// ── sampleBottomUp ────────────────────────────────────────────────────────────
//
// Used by indexChunk. Processes the entire local octree under `node` (no stop
// nodes). Every node calls onNodeComplete. Returns the rejected rows from this
// node (= the overflow that the caller saves to chunk_roots.bin for chunkRoot).

std::vector<uint8_t> Indexer::sampleBottomUp(
    const std::shared_ptr<Node> &node,
    std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints,
    bool isTaskRoot,
    std::vector<uint8_t> *taskRootAccepted) const {
    std::vector<uint8_t> candidates;

    auto it = nodePoints.find(node.get());
    if (it != nodePoints.end()) {
        candidates = std::move(it->second);
        nodePoints.erase(it);
    }

    for (int c = 0; c < 8; ++c) {
        if (!node->children_[c]) continue;
        auto childRejected = sampleBottomUp(node->children_[c], nodePoints, false);
        candidates.insert(candidates.end(), childRejected.begin(), childRejected.end());
    }

    PointBatch batch{candidates.data(),
                     rowStride_ > 0 ? candidates.size() / rowStride_ : 0,
                     rowStride_};
    std::vector<uint8_t> accepted, rejected;
    sampler_->doSample(batch, node, spacingOf(node->level_), accepted, rejected);

    if (isTaskRoot) {
        // Defer onNodeComplete to the caller (mergeChunks) so the correct
        // accepted/overflow split can be applied per case.
        if (taskRootAccepted) *taskRootAccepted = std::move(accepted);
    } else {
        sampler_->onNodeComplete(node, accepted);
    }

    return rejected;
}

// ── sampleSkeleton ────────────────────────────────────────────────────────────
//
// Used by mergeChunks general case. Processes skeleton nodes (above chunk roots).
// At stop nodes (chunk roots): calls onNodeComplete(stopNode, accepted), then
//   returns the overflow as candidates for the parent.
// At skeleton nodes: collects candidates from children, samples, calls
//   onNodeComplete. Returns rejected for the parent.
// At the global root (isRoot=true): folds rejected back into accepted (no parent).
//
// skeletonData maps each chunk root Node* to {accepted, overflow}.

std::vector<uint8_t> Indexer::sampleSkeleton(
    const std::shared_ptr<Node> &node,
    std::unordered_map<Node *, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> &skeletonData,
    const std::unordered_set<Node *> &stopNodes,
    bool isRoot) const {
    if (stopNodes.count(node.get())) {
        // Chunk root: write its accepted (deferred from indexChunk), return overflow
        // as candidates for the parent skeleton node.
        auto &data = skeletonData[node.get()];
        sampler_->onNodeComplete(node, data.first);
        return data.second;
    }

    std::vector<uint8_t> candidates;
    for (int c = 0; c < 8; ++c) {
        if (!node->children_[c]) continue;
        auto childCandidates = sampleSkeleton(node->children_[c], skeletonData, stopNodes, false);
        candidates.insert(candidates.end(), childCandidates.begin(), childCandidates.end());
    }

    PointBatch batch{candidates.data(),
                     rowStride_ > 0 ? candidates.size() / rowStride_ : 0,
                     rowStride_};
    std::vector<uint8_t> accepted, rejected;
    sampler_->doSample(batch, node, spacingOf(node->level_), accepted, rejected);

    if (isRoot) {
        // Global root has no parent — absorb rejected so no points are dropped.
        accepted.insert(accepted.end(), rejected.begin(), rejected.end());
        rejected.clear();
    }

    sampler_->onNodeComplete(node, accepted);
    return rejected;
}

// ── indexChunk ────────────────────────────────────────────────────────────────

bool Indexer::indexChunk(const std::shared_ptr<Node> &chunkRoot, const ChunkInfo &chunk) {
    std::filesystem::path chunkFile =
        std::filesystem::path(targetDir_) / "chunks" / (chunk.name + ".bin");
    std::ifstream in(chunkFile, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    std::streamsize fileSize = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> rows(static_cast<size_t>(fileSize));
    if (!in.read(reinterpret_cast<char *>(rows.data()), fileSize)) return false;

    std::vector<uint32_t> sortedIdx;
    std::vector<uint64_t> codes;
    mortonSort(rows, chunkRoot->bb_, sortedIdx, codes);

    std::unordered_map<Node *, std::vector<uint8_t>> nodePoints;
    size_t n = rowStride_ > 0 ? rows.size() / rowStride_ : 0;
    buildLocalOctree(chunkRoot, rows, sortedIdx, codes,
                     0, n, 0, kMaxLocalDepth, true, nodePoints);

    // Sample all nodes except chunkRoot (task root). onNodeComplete for
    // chunkRoot is deferred to mergeChunks so it can be called once with
    // the correct final accepted set (which depends on whether chunkRoot is
    // the global root or a skeleton node).
    ChunkRootRecord rec;
    rec.nodePtr  = chunkRoot.get();
    rec.overflow = sampleBottomUp(chunkRoot, nodePoints, true, &rec.accepted);

    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        chunkRootRecords_.push_back(std::move(rec));
    }

    return true;
}

// ── mergeChunks ───────────────────────────────────────────────────────────────

bool Indexer::mergeChunks(const std::shared_ptr<Node> &root,
                           const std::vector<ChunkInfo> &chunks) {
    // Collect in-memory accepted + overflow for all chunk roots.
    // accepted was deferred from indexChunk; overflow flows up to skeleton.
    struct ChunkRootData {
        std::vector<uint8_t> accepted;
        std::vector<uint8_t> overflow;
    };
    std::unordered_set<Node *> stopNodes;
    std::unordered_map<Node *, ChunkRootData> nodeData;

    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        for (ChunkRootRecord &cr : chunkRootRecords_) {
            stopNodes.insert(cr.nodePtr);
            nodeData[cr.nodePtr] = {std::move(cr.accepted), std::move(cr.overflow)};
        }
    }

    // Special case: root itself is a chunk root — fold all overflow back into
    // accepted (global root has no parent, no points should be discarded).
    if (stopNodes.count(root.get())) {
        auto &data = nodeData[root.get()];
        data.accepted.insert(data.accepted.end(), data.overflow.begin(), data.overflow.end());
        sampler_->onNodeComplete(root, data.accepted);
        return true;
    }

    // General case: sample skeleton nodes above chunk roots via sampleSkeleton.
    // Build the overflow map that sampleSkeleton reads at stop nodes.
    std::unordered_map<Node *, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> skeletonData;
    for (auto &[nodePtr, data] : nodeData)
        skeletonData[nodePtr] = {std::move(data.accepted), std::move(data.overflow)};

    sampleSkeleton(root, skeletonData, stopNodes, true);
    return true;
}

// ── rebuildIndex ──────────────────────────────────────────────────────────────

bool Indexer::rebuildIndex() {
    namespace fs = std::filesystem;

    // Flush writer before reading header files from disk.
    writer_->flushAll();

    fs::path chunksDir  = fs::path(targetDir_) / "chunks";
    fs::path rootHeader = chunksDir / "r.header.bin";

    // Enumerate batch header files: name is "<batchName>.header.bin" where
    // batchName.size() == stepSize+1. path::stem() only strips the final
    // ".bin", leaving ".header" attached (e.g. "r0020.header.bin" ->
    // "r0020.header", 12 chars) so it can never match stepSize+1 directly —
    // strip the full ".header.bin" suffix explicitly instead. Otherwise this
    // both drops every real batch file (their nodes never reach hierarchy.bin)
    // and misidentifies raw chunk point files (e.g. "r0060.bin", whose stem
    // happens to be stepSize+1 chars) as batch headers, feeding raw point
    // bytes through the named-record parser.
    static constexpr std::string_view kHeaderSuffix = ".header.bin";
    std::vector<fs::path> batchFiles;
    if (fs::exists(chunksDir)) {
        for (const auto &e : fs::directory_iterator(chunksDir)) {
            if (!e.is_regular_file()) continue;
            std::string filename = e.path().filename().string();
            if (filename.size() <= kHeaderSuffix.size()) continue;
            if (filename.compare(filename.size() - kHeaderSuffix.size(),
                                  kHeaderSuffix.size(), kHeaderSuffix) != 0)
                continue;
            std::string batchName = filename.substr(0, filename.size() - kHeaderSuffix.size());
            if (static_cast<uint32_t>(batchName.size()) == options_.stepSize + 1)
                batchFiles.push_back(e.path());
        }
        std::sort(batchFiles.begin(), batchFiles.end());
    }

    // ── Parse named records ───────────────────────────────────────────────────
    // Intermediate header files use format: [uint8 nameLen][name][22-byte record].
    // Parse into (name, 22-byte data) pairs, then sort into BFS order so the
    // viewer's loadHierarchyChunk (which expects root-first BFS) works correctly.

    struct NamedRecord {
        std::string name;
        std::array<uint8_t, 22> data;
    };

    auto readFile = [](const fs::path &p) -> std::vector<uint8_t> {
        std::ifstream in(p, std::ios::binary | std::ios::ate);
        if (!in.is_open()) return {};
        size_t sz = static_cast<size_t>(in.tellg());
        in.seekg(0);
        std::vector<uint8_t> buf(sz);
        in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(sz));
        return buf;
    };

    auto parseNamedSection = [](const std::vector<uint8_t> &sec) {
        std::vector<NamedRecord> records;
        size_t i = 0;
        while (i < sec.size()) {
            if (i + 1 > sec.size()) break;
            uint8_t nameLen = sec[i++];
            if (i + nameLen + 22 > sec.size()) break;
            NamedRecord rec;
            rec.name.assign(reinterpret_cast<const char *>(sec.data() + i), nameLen);
            i += nameLen;
            std::copy(sec.begin() + i, sec.begin() + i + 22, rec.data.begin());
            i += 22;
            records.push_back(std::move(rec));
        }
        return records;
    };

    // BFS comparator: shorter names (= shallower) first; ties broken alphabetically.
    auto bfsLess = [](const NamedRecord &a, const NamedRecord &b) {
        if (a.name.size() != b.name.size()) return a.name.size() < b.name.size();
        return a.name < b.name;
    };

    // Parse root section.
    auto rootRaw = readFile(rootHeader);
    auto rootRecords = parseNamedSection(rootRaw);

    // A root-group node at name.size() == stepSize has children whose names
    // reach stepSize+1, which headerPath() routes to a separate batch file —
    // those children have no record in this chunk even though childMask says
    // they exist. The viewer's BFS-positional parser (loadHierarchyChunk)
    // needs a record at that position to know to lazily fetch the batch
    // chunk, so insert a placeholder Proxy record per such child; the patch
    // step below fills in its real address/size once batch offsets are known.
    {
        std::vector<NamedRecord> proxyPlaceholders;
        for (const auto &rec : rootRecords) {
            if (rec.name.size() != options_.stepSize) continue;
            uint8_t mask = rec.data[1];
            for (int c = 0; c < 8; ++c) {
                if (!((mask >> c) & 1)) continue;
                NamedRecord proxy;
                proxy.name = rec.name + std::to_string(c);
                proxy.data.fill(0);
                proxy.data[0] = static_cast<uint8_t>(NodeType::Proxy);
                proxyPlaceholders.push_back(std::move(proxy));
            }
        }
        rootRecords.insert(rootRecords.end(), proxyPlaceholders.begin(), proxyPlaceholders.end());
    }
    std::sort(rootRecords.begin(), rootRecords.end(), bfsLess);

    // Parse and sort each batch section.
    std::vector<std::vector<NamedRecord>> batchRecords(batchFiles.size());
    for (size_t i = 0; i < batchFiles.size(); ++i) {
        auto raw = readFile(batchFiles[i]);
        batchRecords[i] = parseNamedSection(raw);
        std::sort(batchRecords[i].begin(), batchRecords[i].end(), bfsLess);
    }

    // ── Compute final byte offsets ────────────────────────────────────────────
    constexpr size_t kRecordSize = 22;
    uint64_t rootByteSize = static_cast<uint64_t>(rootRecords.size()) * kRecordSize;

    std::vector<uint64_t> batchOffset(batchFiles.size());
    std::vector<uint64_t> batchByteSize(batchFiles.size());
    uint64_t cursor = rootByteSize;
    for (size_t i = 0; i < batchFiles.size(); ++i) {
        batchOffset[i]   = cursor;
        batchByteSize[i] = static_cast<uint64_t>(batchRecords[i].size()) * kRecordSize;
        cursor += batchByteSize[i];
    }

    // ── Build name→batch index map for Proxy patching ─────────────────────────
    // path::stem() only strips the trailing ".bin", leaving ".header"
    // attached (e.g. "r0020.header.bin".stem() == "r0020.header") — strip
    // the full ".header.bin" suffix explicitly, same as the batchFiles scan
    // above, so this actually matches the 5-char proxy record names.
    std::unordered_map<std::string, size_t> batchStemToIdx;
    for (size_t i = 0; i < batchFiles.size(); ++i) {
        std::string filename = batchFiles[i].filename().string();
        batchStemToIdx[filename.substr(0, filename.size() - kHeaderSuffix.size())] = i;
    }

    // ── Patch Proxy records in root section ───────────────────────────────────
    // Proxy records mark batch-group boundaries; fill in the final offset+size.
    for (auto &rec : rootRecords) {
        if (rec.data[0] != static_cast<uint8_t>(NodeType::Proxy)) continue;
        auto it = batchStemToIdx.find(rec.name);
        if (it == batchStemToIdx.end()) continue;
        size_t bIdx = it->second;
        std::memcpy(rec.data.data() + 6,  &batchOffset[bIdx],   8);
        std::memcpy(rec.data.data() + 14, &batchByteSize[bIdx], 8);
    }

    // ── Assemble and write hierarchy.bin ──────────────────────────────────────
    std::vector<uint8_t> hierarchyBin;
    hierarchyBin.reserve(cursor);
    for (const auto &rec : rootRecords)
        hierarchyBin.insert(hierarchyBin.end(), rec.data.begin(), rec.data.end());
    for (const auto &batch : batchRecords)
        for (const auto &rec : batch)
            hierarchyBin.insert(hierarchyBin.end(), rec.data.begin(), rec.data.end());

    writer_->append("hierarchy.bin", hierarchyBin);

    // ── Update metadata.json ──────────────────────────────────────────────────
    fs::path metaPath = fs::path(targetDir_) / "metadata.json";
    if (fs::exists(metaPath)) {
        std::ifstream metaIn(metaPath);
        nlohmann::json j;
        metaIn >> j;
        metaIn.close();

        j["hierarchy"]["firstChunkSize"] = rootByteSize;
        j["hierarchy"]["depth"]          = maxLevel_;

        std::ofstream metaOut(metaPath);
        metaOut << j.dump(4);
    }

    // ── Clean up intermediate files ───────────────────────────────────────────
    std::error_code ec;
    fs::remove_all(chunksDir, ec);

    return true;
}
