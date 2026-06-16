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

// Holds the sampling output for one chunk root.
// onNodeComplete for the chunk root is intentionally DEFERRED to mergeChunks
// so the correct per-case handling can be applied:
//   - global root (no parent):  write accepted + overflow (fold all back)
//   - other chunk roots:        write accepted, return overflow as candidates
//                               for the parent skeleton node.
struct ChunkRootRecord {
    Node *nodePtr = nullptr;
    std::vector<uint8_t> accepted;   // points that passed doSample at chunkRoot
    std::vector<uint8_t> overflow;   // points rejected by doSample (bubble up)
};

// Aggregates one or more ChunkRootRecords into a batch that can be loaded and
// sampled together (total points <= maxPointsPerChunk).
struct MergeTask {
    Node *taskRoot = nullptr;                   // skeleton node that is the root of this task
    uint64_t totalPoints = 0;
    std::vector<ChunkRootRecord> chunkRoots;    // chunk roots aggregated into this task
};

// Sampling stage (3.3.3 / 3.3.4): builds a Morton-sorted local octree inside
// each chunk, samples it bottom-up (writing to octree.bin and per-chunk
// header.bin files), and saves overflow points to chunk_roots.bin.
// mergeChunks then groups chunk roots into Tasks, loads their overflow points,
// and samples the skeleton nodes above the chunk roots.
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
    // samples it bottom-up. The overflow (rejected from chunkRoot) is appended
    // to "chunk_roots.bin" and recorded in chunkRootRecords_ (thread-safe).
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

    // Post-order traversal: collect candidates, call doSample, then
    // onNodeComplete for every node EXCEPT the task root.
    // For the task root (isTaskRoot=true), stores its accepted in
    // *taskRootAccepted (if non-null) without calling onNodeComplete.
    // Returns the rejected rows (overflow) for the caller.
    std::vector<uint8_t> sampleBottomUp(const std::shared_ptr<Node> &node,
                                         std::unordered_map<Node *, std::vector<uint8_t>> &nodePoints,
                                         bool isTaskRoot,
                                         std::vector<uint8_t> *taskRootAccepted = nullptr) const;

    // Used by mergeChunks (general case). At stop nodes: calls onNodeComplete
    // with the deferred accepted, returns overflow as candidates. At skeleton
    // nodes: samples and calls onNodeComplete. isRoot folds rejected back in.
    std::vector<uint8_t> sampleSkeleton(
        const std::shared_ptr<Node> &node,
        std::unordered_map<Node *, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> &skeletonData,
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
