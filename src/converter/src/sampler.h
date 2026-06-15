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

// A batch of point rows, laid out back-to-back per `attributes.getTotalBytes()`.
struct PointBatch {
    const uint8_t *data;
    uint64_t numPoints;
    uint64_t rowStride;
};

// Decides, for a set of candidate point rows belonging to one octree node,
// which rows stay in that node (accepted) and which are handed back to the
// parent node to compete for a slot there (rejected).
class Sampler {
public:
    virtual ~Sampler() = default;

    virtual void sample(const PointBatch &candidates,
                         const BoundingBoxd &nodeBB,
                         double spacing,
                         const Attributes &attributes,
                         std::vector<uint8_t> &accepted,
                         std::vector<uint8_t> &rejected) const = 0;
};

// Greedy Poisson-disk sampling: a candidate is accepted if its position is at
// least `spacing` away from every already-accepted point, otherwise rejected.
// Uses a uniform grid (cell size = spacing) over candidate positions to avoid
// an all-pairs distance check.
class PoissonDiskSampler : public Sampler {
public:
    void sample(const PointBatch &candidates, const BoundingBoxd &nodeBB, double spacing,
                 const Attributes &attributes,
                 std::vector<uint8_t> &accepted, std::vector<uint8_t> &rejected) const override;
};

// Looks up a Sampler implementation by name (ConverterOptions::sampling).
// Unrecognized names fall back to "poisson".
std::unique_ptr<Sampler> createSampler(const std::string &name);

#endif //PCLITE_SAMPLER_H
