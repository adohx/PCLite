//
// Created by cj on 2026-06-14.
//

#include "indexer.h"

#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>

#include "attribute_handler/attribute_handler_registry.h"
#include "concurrent_writer.h"
#include "octree_naming.h"

namespace {

constexpr size_t kHierarchyRecordBytes = 22;

void appendRecord(std::vector<uint8_t> &out, const std::shared_ptr<Node> &node) {
    size_t base = out.size();
    out.resize(base + kHierarchyRecordBytes);

    uint8_t type = static_cast<uint8_t>(node->type_);
    out[base + 0] = type;
    out[base + 1] = node->childMask_;
    std::memcpy(out.data() + base + 2, &node->numPoints_, 4);
    std::memcpy(out.data() + base + 6, &node->address_, 8);
    std::memcpy(out.data() + base + 14, &node->byteSize_, 8);
}

} // namespace

Indexer::Indexer(Attributes attributes, std::string targetDir, std::shared_ptr<ConcurrentWriter> writer,
                  std::unique_ptr<Sampler> sampler, ConverterOptions options, double rootSpacing)
    : attributes_(std::move(attributes)), targetDir_(std::move(targetDir)), writer_(std::move(writer)),
      sampler_(std::move(sampler)), options_(options), rootSpacing_(rootSpacing) {
    rowStride_ = attributes_.getTotalBytes();
    positionOffset_ = attributes_.getOffset("position");

    for (const Attribute &a : attributes_) {
        if (a.name_ == "position") {
            positionAttr_ = a;
            break;
        }
    }
    positionHandler_ = AttributeHandlerRegistry::get(positionAttr_);
}

double Indexer::spacingOf(int level) const {
    return rootSpacing_ / std::pow(2.0, level);
}

std::shared_ptr<Node> Indexer::buildLocalOctree(const std::shared_ptr<Node> &chunkRoot, uint64_t numPoints) const {
    uint64_t threshold = std::max<uint64_t>(options_.firstChunkSize, 1);

    int depth = 0;
    while ((numPoints >> (3 * depth)) > threshold) ++depth;

    std::vector<std::shared_ptr<Node>> level = {chunkRoot};
    for (int d = 0; d < depth; ++d) {
        std::vector<std::shared_ptr<Node>> next;
        next.reserve(level.size() * 8);

        for (const std::shared_ptr<Node> &node : level) {
            for (int c = 0; c < 8; ++c) {
                BoundingBoxd childBB = octree_naming::childBoundingBox(node->bb_, c);
                auto child = std::make_shared<Node>(node->name_ + std::to_string(c), childBB, node);
                child->level_ = node->level_ + 1;
                node->children_[c] = child;
                next.push_back(child);
            }
        }

        level = std::move(next);
    }

    return chunkRoot;
}

void Indexer::loadAndDistribute(const std::shared_ptr<Node> &localRoot, const ChunkInfo &chunk,
                                 std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const {
    if (rowStride_ == 0) return;

    std::filesystem::path chunkFile = std::filesystem::path(targetDir_) / "chunks" / (chunk.name + ".bin");
    std::ifstream in(chunkFile, std::ios::binary);
    if (!in.is_open()) return;

    std::vector<uint8_t> row(rowStride_);
    while (in.read(reinterpret_cast<char *>(row.data()), static_cast<std::streamsize>(rowStride_))) {
        double posOut[3] = {0, 0, 0};
        positionHandler_->decode(row.data() + positionOffset_, positionAttr_, posOut);
        vec3d p{posOut[0], posOut[1], posOut[2]};

        std::shared_ptr<Node> node = localRoot;
        while (node->children_[0]) {
            int idx = octree_naming::childIndexOf(p, node->bb_);
            node = node->children_[idx];
        }

        std::vector<uint8_t> &buf = nodePoints[node.get()];
        buf.insert(buf.end(), row.begin(), row.end());
    }
}

void Indexer::sampleRecursive(const std::shared_ptr<Node> &node,
                               std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints,
                               const std::unordered_set<Node *> &stopNodes,
                               std::vector<uint8_t> &rejectedOut) const {
    if (stopNodes.find(node.get()) != stopNodes.end()) {
        // Already sampled and flushed by indexChunk(); just hand its leftover
        // (overflow) points up to the parent without touching it again.
        auto it = nodePoints.find(node.get());
        rejectedOut = (it != nodePoints.end()) ? std::move(it->second) : std::vector<uint8_t>();
        if (it != nodePoints.end()) nodePoints.erase(it);
        return;
    }

    std::vector<uint8_t> candidates;

    auto it = nodePoints.find(node.get());
    if (it != nodePoints.end()) {
        candidates = std::move(it->second);
        nodePoints.erase(it);
    }

    for (int c = 0; c < 8; ++c) {
        if (!node->children_[c]) continue;

        std::vector<uint8_t> childRejected;
        sampleRecursive(node->children_[c], nodePoints, stopNodes, childRejected);
        candidates.insert(candidates.end(), childRejected.begin(), childRejected.end());
    }

    PointBatch batch{candidates.data(), rowStride_ > 0 ? candidates.size() / rowStride_ : 0, rowStride_};

    std::vector<uint8_t> accepted, rejected;
    sampler_->sample(batch, node->bb_, spacingOf(node->level_), attributes_, accepted, rejected);

    if (!node->parent_) {
        // Root has no parent to hand rejected points to; keep everything.
        accepted.insert(accepted.end(), rejected.begin(), rejected.end());
        rejected.clear();
    }

    flushNode(node, accepted);
    rejectedOut = std::move(rejected);
}

void Indexer::flushNode(const std::shared_ptr<Node> &node, const std::vector<uint8_t> &acceptedRows) const {
    ConcurrentWriter::WriteResult result = writer_->append("octree.bin", acceptedRows);

    node->address_ = result.offset;
    node->byteSize_ = result.size;
    node->numPoints_ = rowStride_ > 0 ? static_cast<uint32_t>(acceptedRows.size() / rowStride_) : 0;

    for (uint32_t i = 0; i < node->numPoints_; ++i) {
        double posOut[3] = {0, 0, 0};
        positionHandler_->decode(acceptedRows.data() + i * rowStride_ + positionOffset_, positionAttr_, posOut);
        node->tightBB_.expand({posOut[0], posOut[1], posOut[2]});
    }
}

bool Indexer::indexChunk(const std::shared_ptr<Node> &chunkRoot, const ChunkInfo &chunk) {
    std::shared_ptr<Node> localRoot = buildLocalOctree(chunkRoot, chunk.numPoints);

    std::unordered_map<Node *, std::vector<uint8_t>> nodePoints;
    loadAndDistribute(localRoot, chunk, nodePoints);

    std::vector<uint8_t> rejected;
    sampleRecursive(localRoot, nodePoints, {}, rejected);

    std::lock_guard<std::mutex> lock(overflowMutex_);
    chunkOverflow_[chunkRoot.get()] = std::move(rejected);

    return true;
}

void Indexer::collectChunkRoots(const std::shared_ptr<Node> &node,
                                 const std::unordered_set<std::string> &chunkNames,
                                 std::unordered_set<Node *> &outStopNodes,
                                 std::unordered_map<Node *, std::vector<uint8_t>> &outNodePoints) const {
    if (chunkNames.count(node->name_)) {
        outStopNodes.insert(node.get());

        auto it = chunkOverflow_.find(node.get());
        outNodePoints[node.get()] = (it != chunkOverflow_.end()) ? it->second : std::vector<uint8_t>();
        return;
    }

    for (int c = 0; c < 8; ++c) {
        if (node->children_[c]) collectChunkRoots(node->children_[c], chunkNames, outStopNodes, outNodePoints);
    }
}

bool Indexer::mergeChunks(const std::shared_ptr<Node> &root, const std::vector<ChunkInfo> &chunks) {
    std::unordered_set<std::string> chunkNames;
    for (const ChunkInfo &chunk : chunks) chunkNames.insert(chunk.name);

    // If root itself is a chunk root, it was already fully sampled and
    // flushed by indexChunk(); nothing left to merge above it.
    if (!chunkNames.count(root->name_)) {
        std::unordered_map<Node *, std::vector<uint8_t>> nodePoints;
        std::unordered_set<Node *> stopNodes;
        collectChunkRoots(root, chunkNames, stopNodes, nodePoints);

        std::vector<uint8_t> rootRejected;
        sampleRecursive(root, nodePoints, stopNodes, rootRejected);
    }

    writeHierarchy(root);
    return true;
}

void Indexer::writeHierarchy(const std::shared_ptr<Node> &root) {
    maxLevel_ = 0;

    std::vector<uint8_t> records;
    std::deque<std::shared_ptr<Node>> queue = {root};

    while (!queue.empty()) {
        std::shared_ptr<Node> node = queue.front();
        queue.pop_front();

        uint8_t childMask = 0;
        for (int c = 0; c < 8; ++c) {
            if (node->children_[c]) {
                childMask |= static_cast<uint8_t>(1u << c);
                queue.push_back(node->children_[c]);
            }
        }

        node->childMask_ = childMask;
        node->type_ = childMask == 0 ? NodeType::Leaf : NodeType::Normal;
        maxLevel_ = std::max(maxLevel_, node->level_);

        appendRecord(records, node);
    }

    ConcurrentWriter::WriteResult result = writer_->append("hierarchy.bin", records);
    hierarchyByteSize_ = result.size;
}
