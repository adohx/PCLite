//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_SAMPLER_H
#define PCLITE_SAMPLER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "attributes.h"
#include "bounding_box.h"
#include "converter_options.h"
#include "node.h"

class AttributeHandler;
class ConcurrentWriter;

// A batch of point rows, laid out back-to-back per attributes.getTotalBytes().
struct PointBatch {
    const uint8_t *data;
    uint64_t numPoints;
    uint64_t rowStride;
};

// Base class for all point-cloud samplers used by Indexer::indexChunk and
// Indexer::mergeChunks. Subclasses implement doSample(); the shared
// onNodeComplete() writes the results to disk.
//
// Ownership model: one Sampler instance is shared across all concurrent
// indexChunk calls, so doSample() must be thread-safe (stateless or guarded).
// onNodeComplete() serialises through ConcurrentWriter's per-file mutexes.
class Sampler {
public:
    Sampler(std::shared_ptr<ConcurrentWriter> writer,
            std::string targetDir,
            Attributes attributes,
            ConverterOptions options);

    virtual ~Sampler() = default;

    // Called once per node after doSample(). Writes accepted rows to
    // "octree.bin" (filling node->address_/byteSize_/numPoints_/tightBB_)
    // and queues a 22-byte hierarchy record for the appropriate
    // "chunks/<group>.header.bin" (root group vs. batch group per stepSize_)
    // — see queueHeaderWrite().
    void onNodeComplete(const std::shared_ptr<Node> &node,
                        const std::vector<uint8_t> &accepted);

    // Writes whatever hierarchy header records are still buffered (see
    // queueHeaderWrite()). Must be called once after every onNodeComplete()
    // call has finished (i.e. after sampling+merging) and before the header
    // files are read back, e.g. by Indexer::rebuildIndex().
    void flushPendingHeaderWrites();

    // Partition `candidates` into accepted (promoted to this node, to be
    // re-tested by its own parent) and rejected (settles back at whichever
    // child contributed it). `acceptFlags` marks each candidate row 1 (in
    // `accepted`) or 0 (in `rejected`), in original input order, so the
    // caller can route rejected rows back to their originating child without
    // having to re-run the spacing test itself. Must be thread-safe.
    virtual void doSample(const PointBatch &candidates,
                          const std::shared_ptr<Node> &node,
                          double spacing,
                          std::vector<uint8_t> &accepted,
                          std::vector<uint8_t> &rejected,
                          std::vector<uint8_t> &acceptFlags) = 0;

protected:
    std::shared_ptr<ConcurrentWriter> writer_;
    std::string targetDir_;
    Attributes attributes_;
    ConverterOptions options_;

    uint64_t rowStride_ = 0;
    uint64_t posOffset_ = 0;
    Attribute posAttr_;
    const AttributeHandler *posHandler_ = nullptr;

private:
    // Hierarchy header records (one per finalised node) are batched in
    // memory and flushed together via ConcurrentWriter::appendAndClose(),
    // instead of routing each one through ConcurrentWriter::append() (which
    // would hold one open file handle per distinct batch path — there can be
    // tens of thousands of them — for the entire sampling/merging phase, and
    // pay for closing all of them at once in flushAll()). Mirrors
    // PotreeConverter's HierarchyFlusher (Converter/include/indexer.h).
    struct PendingHeaderWrite {
        std::string path;
        std::vector<uint8_t> bytes; // [uint8 nameLen][name][22-byte record]
    };

    static constexpr size_t kHeaderFlushThreshold = 10'000;

    void queueHeaderWrite(std::string path, std::vector<uint8_t> bytes);
    void flushHeaderWrites(const std::vector<PendingHeaderWrite> &writes);

    std::mutex headerMutex_;
    std::vector<PendingHeaderWrite> pendingHeaderWrites_;
};

// Keeps every K-th point (uniform stride), rejecting the rest.
class RandomSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected, std::vector<uint8_t> &acceptFlags) override;
};

// Greedy Poisson-disk: a candidate is accepted if its position is at least
// `spacing` away from every already-accepted point (grid-accelerated).
class PoissonDiskSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected, std::vector<uint8_t> &acceptFlags) override;
};

// Poisson-disk with flag-array + bulk-copy: maintains a Morton-sorted accepted
// set for fast neighbourhood queries; marks each candidate accept/reject via a
// bool flag, then copies memory in one pass.
class PoissonAverageSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected, std::vector<uint8_t> &acceptFlags) override;
};

// Factory: "random" -> RandomSampler, "poisson_average" -> PoissonAverageSampler,
// anything else -> PoissonDiskSampler.
std::unique_ptr<Sampler> createSampler(const std::string &name,
                                        std::shared_ptr<ConcurrentWriter> writer,
                                        std::string targetDir,
                                        Attributes attributes,
                                        ConverterOptions options);

#endif //PCLITE_SAMPLER_H
