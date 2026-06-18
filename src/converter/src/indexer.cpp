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

namespace {

// One contiguous row-range of `candidates`, contributed by child octant `c`.
struct ChildRange {
    int c;
    size_t begin, end; // row indices, in candidates' own units (not bytes)
};

// Splits `rejected` rows (per `flags`, in the same row order as `candidates`)
// back to whichever child contributed each row, and finalises that child via
// onNodeComplete — this is the child's FINAL data: it is never touched again.
void finalizeRejectedChildren(Sampler &sampler,
                               const std::shared_ptr<Node> &node,
                               const std::vector<uint8_t> &candidates,
                               uint64_t rowStride,
                               const std::vector<uint8_t> &flags,
                               const std::vector<ChildRange> &ranges) {
    for (const ChildRange &cr : ranges) {
        std::vector<uint8_t> childRejected;
        for (size_t i = cr.begin; i < cr.end; ++i) {
            if (flags[i]) continue;
            const uint8_t *row = candidates.data() + i * rowStride;
            childRejected.insert(childRejected.end(), row, row + rowStride);
        }
        sampler.onNodeComplete(node->children_[cr.c], childRejected);
    }
}

} // namespace

// ── sampleBottomUp ────────────────────────────────────────────────────────────
//
// Used by indexChunk. Post-order traversal matching PotreeConverter's
// direction: accepted points are promoted UP (to be re-tested by this node's
// own parent later); rejected points settle back DOWN at whichever child
// offered them (that child's final, on-disk representation). A structural
// leaf (no children) is never sampled — its pre-assigned points are returned
// untouched, exactly as PotreeConverter leaves leaf nodes alone.

std::vector<uint8_t> Indexer::sampleBottomUp(
    const std::shared_ptr<Node> &node,
    std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const {
    bool hasChildren = false;
    for (int c = 0; c < 8; ++c) {
        if (node->children_[c]) { hasChildren = true; break; }
    }

    if (!hasChildren) {
        auto it = nodePoints.find(node.get());
        if (it == nodePoints.end()) return {};
        std::vector<uint8_t> pts = std::move(it->second);
        nodePoints.erase(it);
        return pts;
    }

    std::vector<uint8_t> candidates;
    std::vector<ChildRange> ranges;
    for (int c = 0; c < 8; ++c) {
        if (!node->children_[c]) continue;
        auto childPromotable = sampleBottomUp(node->children_[c], nodePoints);
        size_t begin = rowStride_ > 0 ? candidates.size() / rowStride_ : 0;
        candidates.insert(candidates.end(), childPromotable.begin(), childPromotable.end());
        size_t end = rowStride_ > 0 ? candidates.size() / rowStride_ : 0;
        ranges.push_back({c, begin, end});
    }

    PointBatch batch{candidates.data(),
                     rowStride_ > 0 ? candidates.size() / rowStride_ : 0,
                     rowStride_};
    std::vector<uint8_t> accepted, rejected, flags;
    sampler_->doSample(batch, node, spacingOf(node->level_), accepted, rejected, flags);

    finalizeRejectedChildren(*sampler_, node, candidates, rowStride_, flags, ranges);

    return accepted; // promotable set, for this node's own parent to test
}

// ── sampleSkeleton ────────────────────────────────────────────────────────────
//
// Used by mergeChunks general case. Processes skeleton nodes (above chunk
// roots) with the same bottom-up direction as sampleBottomUp: accepted moves
// up, rejected settles back down into whichever child offered it.
// At stop nodes (chunk roots): returns the promotable set computed earlier by
//   indexChunk; finalisation is deferred to this node's own parent, same as
//   any other node.
// At the global root (isRoot=true): there's no parent left to test the
//   accepted set further, so it's written directly as the root's final data.
//
// chunkRootPromotable maps each chunk root Node* to its promotable set.

std::vector<uint8_t> Indexer::sampleSkeleton(
    const std::shared_ptr<Node> &node,
    std::unordered_map<Node *, std::vector<uint8_t>> &chunkRootPromotable,
    const std::unordered_set<Node *> &stopNodes,
    bool isRoot) const {
    if (stopNodes.count(node.get())) {
        auto it = chunkRootPromotable.find(node.get());
        return it != chunkRootPromotable.end() ? std::move(it->second) : std::vector<uint8_t>{};
    }

    std::vector<uint8_t> candidates;
    std::vector<ChildRange> ranges;
    for (int c = 0; c < 8; ++c) {
        if (!node->children_[c]) continue;
        auto childPromotable = sampleSkeleton(node->children_[c], chunkRootPromotable, stopNodes, false);
        size_t begin = rowStride_ > 0 ? candidates.size() / rowStride_ : 0;
        candidates.insert(candidates.end(), childPromotable.begin(), childPromotable.end());
        size_t end = rowStride_ > 0 ? candidates.size() / rowStride_ : 0;
        ranges.push_back({c, begin, end});
    }

    PointBatch batch{candidates.data(),
                     rowStride_ > 0 ? candidates.size() / rowStride_ : 0,
                     rowStride_};
    std::vector<uint8_t> accepted, rejected, flags;
    sampler_->doSample(batch, node, spacingOf(node->level_), accepted, rejected, flags);

    finalizeRejectedChildren(*sampler_, node, candidates, rowStride_, flags, ranges);

    if (isRoot) {
        sampler_->onNodeComplete(node, accepted);
        return {};
    }

    return accepted; // promotable set, for this node's own parent to test
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

    // Sample every descendant of chunkRoot bottom-up. chunkRoot itself is
    // never finalised here — onNodeComplete for it is deferred to
    // mergeChunks, since whether it's the global root (write as-is) or sits
    // under a skeleton (may be tested/rejected further) isn't known yet.
    ChunkRootRecord rec;
    rec.nodePtr    = chunkRoot.get();
    rec.promotable = sampleBottomUp(chunkRoot, nodePoints);

    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        chunkRootRecords_.push_back(std::move(rec));
    }

    return true;
}

// ── mergeChunks ───────────────────────────────────────────────────────────────

bool Indexer::mergeChunks(const std::shared_ptr<Node> &root,
                           const std::vector<ChunkInfo> &chunks) {
    // Collect each chunk root's promotable set (computed by indexChunk).
    std::unordered_set<Node *> stopNodes;
    std::unordered_map<Node *, std::vector<uint8_t>> chunkRootPromotable;

    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        for (ChunkRootRecord &cr : chunkRootRecords_) {
            stopNodes.insert(cr.nodePtr);
            chunkRootPromotable[cr.nodePtr] = std::move(cr.promotable);
        }
    }

    // Special case: root itself is a chunk root (the whole dataset fit in
    // one chunk, no skeleton above it) — its promotable set IS its final
    // data, there's no parent left to test it against.
    if (stopNodes.count(root.get())) {
        sampler_->onNodeComplete(root, chunkRootPromotable[root.get()]);
        return true;
    }

    // General case: sample skeleton nodes above chunk roots via sampleSkeleton.
    sampleSkeleton(root, chunkRootPromotable, stopNodes, true);
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
    // batchName != "r" and (batchName.size()-1) is a multiple of stepSize —
    // i.e. its depth sits exactly on a recursive hierarchy-batch boundary
    // (stepSize, 2*stepSize, 3*stepSize, ...), matching headerPath()'s
    // routing. PotreeConverter recurses this batching throughout the whole
    // tree rather than just once below the root, so this enumerates batches
    // at every depth, not only the first level.
    // path::stem() only strips the final ".bin", leaving ".header" attached
    // (e.g. "r0020.header.bin" -> "r0020.header", 12 chars) so it can never
    // match directly — strip the full ".header.bin" suffix explicitly
    // instead. Otherwise this both drops every real batch file (their nodes
    // never reach hierarchy.bin) and misidentifies raw chunk point files
    // (e.g. "r0060.bin", whose stem happens to be the right length) as batch
    // headers, feeding raw point bytes through the named-record parser.
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
            if (batchName == "r") continue;
            if ((static_cast<uint32_t>(batchName.size()) - 1) % options_.stepSize == 0)
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

    // A batch's own key sits at some depth `keyDepth` (0 for the root batch,
    // keyed by "r"). Its deepest level (depth == keyDepth+stepSize-1, i.e.
    // name.size() == keyDepth+stepSize) has children whose names reach one
    // level deeper, which headerPath() routes to a new batch keyed by that
    // child's own name — those children have no record in THIS batch even
    // though childMask says they exist. The viewer's BFS-positional parser
    // (loadHierarchyChunk) needs a record at that position to know to lazily
    // fetch the next batch, so insert a placeholder Proxy record per such
    // child; the patch step below fills in its real address/size once every
    // batch's offset is known. This mirrors PotreeConverter's recursive
    // hierarchyStepSize batching, applied at every depth boundary, not just
    // once below the root.
    auto insertProxyPlaceholders = [&](std::vector<NamedRecord> &records, size_t keyDepth) {
        std::vector<NamedRecord> proxyPlaceholders;
        for (const auto &rec : records) {
            if (rec.name.size() != keyDepth + options_.stepSize) continue;
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
        records.insert(records.end(), proxyPlaceholders.begin(), proxyPlaceholders.end());
    };

    // Parse root section.
    auto rootRaw = readFile(rootHeader);
    auto rootRecords = parseNamedSection(rootRaw);
    insertProxyPlaceholders(rootRecords, 0);
    std::sort(rootRecords.begin(), rootRecords.end(), bfsLess);

    // Parse and sort each batch section.
    std::vector<std::vector<NamedRecord>> batchRecords(batchFiles.size());
    for (size_t i = 0; i < batchFiles.size(); ++i) {
        auto raw = readFile(batchFiles[i]);
        batchRecords[i] = parseNamedSection(raw);
        std::string filename = batchFiles[i].filename().string();
        std::string batchName = filename.substr(0, filename.size() - kHeaderSuffix.size());
        insertProxyPlaceholders(batchRecords[i], batchName.size() - 1);
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

    // ── Patch Proxy records in every batch (root + all recursive batches) ─────
    // Proxy records mark batch-group boundaries; fill in the final offset+size.
    auto patchProxies = [&](std::vector<NamedRecord> &records) {
        for (auto &rec : records) {
            if (rec.data[0] != static_cast<uint8_t>(NodeType::Proxy)) continue;
            auto it = batchStemToIdx.find(rec.name);
            if (it == batchStemToIdx.end()) continue;
            size_t bIdx = it->second;
            std::memcpy(rec.data.data() + 6,  &batchOffset[bIdx],   8);
            std::memcpy(rec.data.data() + 14, &batchByteSize[bIdx], 8);
        }
    };
    patchProxies(rootRecords);
    for (auto &batch : batchRecords) patchProxies(batch);

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
