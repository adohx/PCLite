//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_INDEXER_H
#define PCLITE_INDEXER_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "attributes.h"
#include "bounding_box.h"
#include "chunker.h"
#include "converter_options.h"
#include "node.h"
#include "sampler.h"

class AttributeHandler;
class ConcurrentWriter;

// Holds the sampling output for one chunk root: its "promotable" point set,
// i.e. what indexChunk's bottom-up sampling decided to keep at the chunk
// root's own level rather than settle back into one of its descendants.
// onNodeComplete for the chunk root itself is intentionally DEFERRED to
// mergeChunks, since whether it's the global root (write as-is) or sits
// under a skeleton (gets tested again, possibly rejected back into) isn't
// known until the skeleton is walked.
struct ChunkRootRecord {
    Node *nodePtr = nullptr;
    std::vector<uint8_t> promotable;
};

// Aggregates one or more ChunkRootRecords into a batch that can be loaded and
// sampled together (total points <= maxPointsPerChunk).
struct MergeTask {
    Node *taskRoot = nullptr;                   // skeleton node that is the root of this task
    uint64_t totalPoints = 0;
    std::vector<ChunkRootRecord> chunkRoots;    // chunk roots aggregated into this task
};

// Sampling stage (3.3.3 / 3.3.4): builds a Morton-sorted local octree inside
// each chunk and samples it bottom-up (writing to octree.bin and per-chunk
// header.bin files), recording each chunk root's promotable point set in
// chunkRootRecords_. mergeChunks then samples the skeleton nodes above the
// chunk roots, with each chunk root's promotable set as its candidate input.
// rebuildIndex finally assembles the global hierarchy.bin.
//
// MergeTask/MergeNode/buildMergeTree/collapseToTasks/processTask below are
// declared but currently unused — mergeChunks samples the whole skeleton in
// one pass via sampleSkeleton rather than batching it into memory-bounded
// Tasks.
class Indexer {
public:
    Indexer(Attributes attributes,
            std::string targetDir,
            std::shared_ptr<ConcurrentWriter> writer,
            std::unique_ptr<Sampler> sampler,
            ConverterOptions options,
            double rootSpacing);

    // Builds the Morton-sorted local octree for this chunk under chunkRoot,
    // samples it bottom-up, and records chunkRoot's promotable point set in
    // chunkRootRecords_ (thread-safe). chunkRoot itself is finalised later,
    // by mergeChunks.
    bool indexChunk(const std::shared_ptr<Node> &chunkRoot, const ChunkInfo &chunk);

    // Groups the chunk roots (by total-point batching) into MergeTasks, loads
    // overflow points from chunk_roots.bin for each Task, rebuilds a local
    // Morton octree, and samples the skeleton nodes above the chunk roots.
    bool mergeChunks(const std::shared_ptr<Node> &root, const std::vector<ChunkInfo> &chunks);

    // Assembles hierarchy.bin from the per-chunk header.bin files: root-group
    // records (chunks/r.header.bin) with Proxy nodes at the batch boundary,
    // followed by each batch's header.bin. Deletes the chunks/ directory.
    bool rebuildIndex();

    int maxLevel() const { return maxLevel_; }

private:
    // ── indexChunk helpers ────────────────────────────────────────────────────

    // Sort `rows` (N*rowStride_ bytes) by Morton code within `bb`; fills
    // `sortedIdx` (indices into rows) and `codes`.
    void mortonSort(const std::vector<uint8_t> &rows, const BoundingBoxd &bb,
                    std::vector<uint32_t> &sortedIdx, std::vector<uint64_t> &codes) const;

    // Recursively build the local octree under `node` from the sorted range
    // [begin,end) in sortedIdx/codes. If allowRefinement is true and a leaf
    // exceeds the firstChunkSize threshold, re-invokes itself from level 0
    // (adaptive depth / Refinement). Assigns leaf point ranges into nodePoints.
    void buildLocalOctree(const std::shared_ptr<Node> &node,
                          const std::vector<uint8_t> &rows,
                          const std::vector<uint32_t> &sortedIdx,
                          const std::vector<uint64_t> &codes,
                          size_t begin, size_t end,
                          int level, int maxDepth,
                          bool allowRefinement,
                          std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const;

    // Post-order traversal matching PotreeConverter's bottom-up sampling
    // direction: a structural leaf (no children) returns its pre-assigned
    // points untouched, with no onNodeComplete call yet — that happens when
    // its parent decides what to reject back into it. An internal node
    // gathers each child's promotable set (recursively), samples at its own
    // level, immediately finalises every child via onNodeComplete with
    // whatever was rejected back to it, and returns its own accepted subset
    // upward as ITS promotable set (to be tested again by its own parent).
    // The node itself is never finalised here — that's always the caller's
    // responsibility (mirroring how a node's final data is only known once
    // its parent's accept/reject decision is made).
    std::vector<uint8_t> sampleBottomUp(
        const std::shared_ptr<Node> &node,
        std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints) const;

    // Used by mergeChunks (general case). At stop nodes (chunk roots): returns
    // the promotable set computed earlier by indexChunk, deferring
    // finalisation to this node's own parent (same as sampleBottomUp).
    // At skeleton nodes: same bottom-up gather/sample/finalise-children
    // behaviour as sampleBottomUp. At the global root (isRoot=true), there's
    // no parent to test the accepted set further, so it's written directly.
    std::vector<uint8_t> sampleSkeleton(
        const std::shared_ptr<Node> &node,
        std::unordered_map<Node *, std::vector<uint8_t>> &chunkRootPromotable,
        const std::unordered_set<Node *> &stopNodes,
        bool isRoot) const;

    double spacingOf(int level) const { return rootSpacing_ / std::pow(2.0, level); }

    // ── mergeChunks helpers ───────────────────────────────────────────────────

    // Mirror the skeleton between `node` and the chunk-root layer into a
    // MergeTask tree, with each chunk root as a leaf task (single chunkRoot).
    // chunkNameSet is the set of chunk names at the chunk-root layer.
    struct MergeNode {
        Node *skelNode = nullptr;
        uint64_t totalPoints = 0;
        std::vector<ChunkRootRecord> chunkRoots;
        std::vector<std::shared_ptr<MergeNode>> children;
    };

    std::shared_ptr<MergeNode> buildMergeTree(
        const std::shared_ptr<Node> &skelNode,
        const std::unordered_map<std::string, ChunkRootRecord> &recordByName) const;

    // Bottom-up collapse: if a MergeNode's total points < threshold, absorb
    // children's chunkRoots and delete children. Returns the flat list of Tasks.
    std::vector<MergeTask> collapseToTasks(const std::shared_ptr<MergeNode> &root) const;

    // Load overflow points for all chunkRoots in a Task from chunk_roots.bin,
    // distribute them into the local octree under taskRoot, sample bottom-up.
    bool processTask(const MergeTask &task);

private:
    Attributes attributes_;
    std::string targetDir_;
    std::shared_ptr<ConcurrentWriter> writer_;
    std::unique_ptr<Sampler> sampler_;
    ConverterOptions options_;
    double rootSpacing_;

    uint64_t rowStride_ = 0;
    uint64_t posOffset_ = 0;
    Attribute posAttr_;
    const AttributeHandler *posHandler_ = nullptr;

    mutable std::mutex recordsMutex_;
    std::vector<ChunkRootRecord> chunkRootRecords_;  // filled by indexChunk (thread-safe)

    mutable int maxLevel_ = 0;
};

#endif //PCLITE_INDEXER_H
