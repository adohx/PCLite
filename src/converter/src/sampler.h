//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_SAMPLER_H
#define PCLITE_SAMPLER_H

#include <cstdint>
#include <memory>
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
    // and appends a 22-byte hierarchy record to the appropriate
    // "chunks/<group>.header.bin" (root group vs. batch group per stepSize_).
    void onNodeComplete(const std::shared_ptr<Node> &node,
                        const std::vector<uint8_t> &accepted);

    // Partition `candidates` into accepted (stay at this node) and rejected
    // (bubble up to parent). Must be thread-safe.
    virtual void doSample(const PointBatch &candidates,
                          const std::shared_ptr<Node> &node,
                          double spacing,
                          std::vector<uint8_t> &accepted,
                          std::vector<uint8_t> &rejected) = 0;

protected:
    std::shared_ptr<ConcurrentWriter> writer_;
    std::string targetDir_;
    Attributes attributes_;
    ConverterOptions options_;

    uint64_t rowStride_ = 0;
    uint64_t posOffset_ = 0;
    Attribute posAttr_;
    const AttributeHandler *posHandler_ = nullptr;
};

// Keeps every K-th point (uniform stride), rejecting the rest.
class RandomSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected) override;
};

// Greedy Poisson-disk: a candidate is accepted if its position is at least
// `spacing` away from every already-accepted point (grid-accelerated).
class PoissonDiskSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected) override;
};

// Poisson-disk with flag-array + bulk-copy: maintains a Morton-sorted accepted
// set for fast neighbourhood queries; marks each candidate accept/reject via a
// bool flag, then copies memory in one pass.
class PoissonAverageSampler : public Sampler {
public:
    using Sampler::Sampler;

    void doSample(const PointBatch &candidates, const std::shared_ptr<Node> &node,
                  double spacing, std::vector<uint8_t> &accepted,
                  std::vector<uint8_t> &rejected) override;
};

// Factory: "random" -> RandomSampler, "poisson_average" -> PoissonAverageSampler,
// anything else -> PoissonDiskSampler.
std::unique_ptr<Sampler> createSampler(const std::string &name,
                                        std::shared_ptr<ConcurrentWriter> writer,
                                        std::string targetDir,
                                        Attributes attributes,
                                        ConverterOptions options);

#endif //PCLITE_SAMPLER_H
