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

// Holds the sampling output for one chunk root: a reference to its
// "promotable" point set on disk (chunks/chunk_roots.bin), i.e. what
// indexChunk's bottom-up sampling decided to keep at the chunk root's own
// level rather than settle back into one of its descendants. The bytes
// themselves are NOT kept in memory — indexChunk writes them to
// "chunk_roots.bin" immediately and frees them, so mergeChunks can process
// chunk roots in memory-bounded batches (see MergeTask) instead of holding
// every chunk root's promotable set in memory at once.
// onNodeComplete for the chunk root itself is intentionally DEFERRED to
// mergeChunks, since whether it's the global root (write as-is) or sits
// under a skeleton (gets tested again, possibly rejected back into) isn't
// known until the skeleton is walked.
struct ChunkRootRecord {
    Node *nodePtr = nullptr;
    uint64_t offset = 0;   // byte offset into "chunk_roots.bin"
    uint64_t byteSize = 0; // byte size of the promotable set at that offset
};

// One batch of chunk roots small enough (totalPoints < kMergeTaskPointThreshold,
// see indexer.cpp) to reload and sample together without holding every chunk
// root's promotable set in memory simultaneously. Mirrors PotreeConverter's
// flushChunkRoot/processChunkRoots/reloadChunkRoots batching
// (Converter/src/indexer.cpp in the reference checkout, see
// reference_potree_converter memory).
struct MergeTask {
    std::shared_ptr<Node> taskRoot;             // skeleton node that is the root of this task
    uint64_t totalPoints = 0;
    std::vector<ChunkRootRecord> chunkRoots;    // chunk roots aggregated into this task
};

// Sampling stage (3.3.3 / 3.3.4): builds a Morton-sorted local octree inside
// each chunk and samples it bottom-up (writing to octree.bin and per-chunk
// header.bin files), recording a disk reference to each chunk root's
// promotable point set in chunkRootRecords_ (the bytes live in
// "chunk_roots.bin", not in memory). mergeChunks groups chunk roots into
// memory-bounded MergeTasks, processes each one (reloading just that task's
// chunk roots), then samples whatever skeleton remains above all task roots.
// rebuildIndex finally assembles the global hierarchy.bin.
class Indexer {
public:
    Indexer(Attributes attributes,
            std::string targetDir,
            std::shared_ptr<ConcurrentWriter> writer,
            std::unique_ptr<Sampler> sampler,
            ConverterOptions options,
            double rootSpacing);

    // Builds the Morton-sorted local octree for this chunk under chunkRoot,
    // samples it bottom-up, writes chunkRoot's promotable point set to
    // "chunk_roots.bin", and records a disk reference to it in
    // chunkRootRecords_ (thread-safe). chunkRoot itself is finalised later,
    // by mergeChunks.
    bool indexChunk(const std::shared_ptr<Node> &chunkRoot, const ChunkInfo &chunk);

    // Groups chunkRootRecords_ into memory-bounded MergeTasks (buildMergeTree
    // + collapseToTasks), processes each one (processTask: reload just that
    // task's chunk roots from "chunk_roots.bin", sample), then samples
    // whatever skeleton remains above all task roots — treating each task
    // root's result as a stand-in chunk root for that final pass. If
    // everything collapsed into a single task rooted at the global root,
    // that task's own sampleSkeleton call already wrote root's final data
    // and no further pass is needed.
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

    // Mirrors the skeleton from `skelNode` down into a MergeNode tree: a node
    // found in `recordByNode` (a chunk root) becomes a MergeNode leaf holding
    // that one ChunkRootRecord, with no further recursion into its
    // descendants (they're already folded into its promotable set by
    // indexChunk); any other node recurses into its existing children and
    // sums their totalPoints.
    struct MergeNode {
        std::shared_ptr<Node> skelNode;
        uint64_t totalPoints = 0;
        std::vector<ChunkRootRecord> chunkRoots;
        std::vector<std::shared_ptr<MergeNode>> children;
    };

    std::shared_ptr<MergeNode> buildMergeTree(
        const std::shared_ptr<Node> &skelNode,
        const std::unordered_map<Node *, ChunkRootRecord> &recordByNode) const;

    // Bottom-up collapse: if a MergeNode has no chunkRoots of its own (i.e.
    // it's a true skeleton node, not itself a chunk root) and its
    // totalPoints < kMergeTaskPointThreshold, absorb every child's
    // chunkRoots into itself and discard the children. Afterwards,
    // chunkRoots.empty() == children.empty() never holds for an internal
    // node — every surviving MergeNode with chunkRoots set is a Task; flattens
    // those into the returned list (depth-first, no further recursion below a
    // Task per the invariant above).
    std::vector<MergeTask> collapseToTasks(const std::shared_ptr<MergeNode> &root) const;

    // Reloads the promotable bytes for every chunk root in `task` from
    // "chunk_roots.bin" (batched, just this task's chunk roots — the whole
    // point of Tasks is to never need every chunk root's bytes in memory at
    // once), then samples task.taskRoot's subtree via sampleSkeleton with
    // those chunk roots as stop nodes. If task.taskRoot IS one of its own
    // chunk roots (a trivial single-leaf task that was never merged with
    // siblings), sampleSkeleton's stop-node short-circuit makes this a
    // pass-through, as intended. Returns task.taskRoot's resulting promotable
    // set (empty if task.taskRoot is the global root — onNodeComplete was
    // already called for it directly, since it has no parent left to test
    // the result further).
    std::vector<uint8_t> processTask(const std::shared_ptr<Node> &root, const MergeTask &task) const;

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
