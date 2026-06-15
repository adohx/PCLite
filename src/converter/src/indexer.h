//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_INDEXER_H
#define PCLITE_INDEXER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "attribute_handler/attribute_handler.h"
#include "attributes.h"
#include "chunker.h"
#include "converter_options.h"
#include "node.h"
#include "sampler.h"

class ConcurrentWriter;

// Per-chunk sampling (3.2.8) plus cross-chunk merge (3.3.4, simplified per the
// design clarifications): builds a local octree under each chunk-root node,
// distributes that chunk's points into it, and samples bottom-up so every
// node ends up with accepted points written to a single global "octree.bin"
// (address_/byteSize_/numPoints_/tightBB_ filled in). mergeChunks() then
// repeats the bottom-up sampling for the skeleton tree above the chunk roots
// (using each chunk's leftover/rejected points), and finally writes a single
// global "hierarchy.bin" (22-byte records, see 2.2) for the whole tree.
class Indexer {
public:
    Indexer(Attributes attributes,
            std::string targetDir,
            std::shared_ptr<ConcurrentWriter> writer,
            std::unique_ptr<Sampler> sampler,
            ConverterOptions options,
            double rootSpacing);

    // Builds and samples chunk's local octree under chunkRoot. Safe to call
    // concurrently for different chunks (each touches disjoint Node objects
    // and a distinct region of "octree.bin"/its own entry in the overflow map).
    bool indexChunk(const std::shared_ptr<Node> &chunkRoot, const ChunkInfo &chunk);

    // Samples the skeleton tree above the chunk-root nodes (identified by
    // chunks[*].name) using each chunk's leftover points, then writes
    // "hierarchy.bin" for the whole tree (root down through chunk interiors).
    bool mergeChunks(const std::shared_ptr<Node> &root, const std::vector<ChunkInfo> &chunks);

    // Valid after mergeChunks(): the maximum Node::level_ found in the tree,
    // for backfilling metadata.json's hierarchy.depth.
    int maxLevel() const { return maxLevel_; }

    // Valid after mergeChunks(): total bytes written to "hierarchy.bin", for
    // backfilling metadata.json's hierarchy.firstChunkSize (the whole index
    // tree is loaded as a single chunk in this version).
    uint64_t hierarchyByteSize() const { return hierarchyByteSize_; }

private:
    // Subdivides chunkRoot down to a depth such that each leaf is expected to
    // hold roughly options_.firstChunkSize points (numPoints / 8^depth <=
    // firstChunkSize), creating all 8 children at every level in between.
    // Returns chunkRoot itself (the root of this local octree).
    std::shared_ptr<Node> buildLocalOctree(const std::shared_ptr<Node> &chunkRoot, uint64_t numPoints) const;

    // Streams "chunks/<chunk.name>.bin" and assigns each row to the leaf of
    // localRoot's subtree whose cell contains the row's position.
    void loadAndDistribute(const std::shared_ptr<Node> &localRoot, const ChunkInfo &chunk,
                            std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const;

    // Post-order: for nodes in `stopNodes`, skip recursion and sample using
    // only nodePoints[node] (pre-populated by the caller); for other nodes,
    // gather nodePoints[node] plus every child's rejected rows, then sample.
    // A node with no parent_ (the global root) has nowhere to send rejected
    // rows, so they are folded back into accepted instead of being returned.
    void sampleRecursive(const std::shared_ptr<Node> &node,
                          std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints,
                          const std::unordered_set<Node *> &stopNodes,
                          std::vector<uint8_t> &rejectedOut) const;

    // Appends acceptedRows to "octree.bin" and fills node's
    // address_/byteSize_/numPoints_/tightBB_ from the result.
    void flushNode(const std::shared_ptr<Node> &node, const std::vector<uint8_t> &acceptedRows) const;

    // = rootSpacing_ / 2^level
    double spacingOf(int level) const;

    // Finds the nodes under `node` whose name_ is in chunkNames (chunk-root
    // nodes), marking them in outStopNodes and seeding outNodePoints from
    // chunkOverflow_. Assumes chunk names never nest (a chunk-root is never a
    // descendant of another chunk-root), so recursion stops once found.
    void collectChunkRoots(const std::shared_ptr<Node> &node,
                            const std::unordered_set<std::string> &chunkNames,
                            std::unordered_set<Node *> &outStopNodes,
                            std::unordered_map<Node *, std::vector<uint8_t>> &outNodePoints) const;

    // Breadth-first traversal of the final tree: fills childMask_/type_,
    // tracks maxLevel_, and serializes every node as a 22-byte record (2.2)
    // into a single "hierarchy.bin".
    void writeHierarchy(const std::shared_ptr<Node> &root);

private:
    Attributes attributes_;
    std::string targetDir_;
    std::shared_ptr<ConcurrentWriter> writer_;
    std::unique_ptr<Sampler> sampler_;
    ConverterOptions options_;
    double rootSpacing_;

    uint64_t rowStride_ = 0;
    uint64_t positionOffset_ = 0;
    Attribute positionAttr_;
    const AttributeHandler *positionHandler_ = nullptr;

    mutable std::mutex overflowMutex_;
    std::unordered_map<Node *, std::vector<uint8_t>> chunkOverflow_;

    int maxLevel_ = 0;
    uint64_t hierarchyByteSize_ = 0;
};

#endif //PCLITE_INDEXER_H
