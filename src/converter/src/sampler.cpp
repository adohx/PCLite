//
// Created by cj on 2026-06-14.
//

#include "sampler.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include "attribute_handler/attribute_handler.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "concurrent_writer.h"
#include "kdtree.h"
#include "octree_naming.h"

namespace {

constexpr size_t kHierarchyRecordBytes = 22;

// Serialise a named hierarchy record for `node` into `out`.
// Format: [uint8 nameLen][name bytes][22-byte record]
// The name prefix lets rebuildIndex sort records into BFS order.
void appendHierarchyRecord(std::vector<uint8_t> &out, const std::shared_ptr<Node> &node) {
    const std::string &name = node->name_;
    auto nameLen = static_cast<uint8_t>(std::min(name.size(), size_t(255)));
    out.push_back(nameLen);
    out.insert(out.end(), name.begin(), name.begin() + nameLen);

    size_t base = out.size();
    out.resize(base + kHierarchyRecordBytes);
    out[base + 0] = static_cast<uint8_t>(node->type_);
    out[base + 1] = node->childMask_;
    std::memcpy(out.data() + base + 2, &node->numPoints_, 4);
    std::memcpy(out.data() + base + 6, &node->address_,  8);
    std::memcpy(out.data() + base + 14, &node->byteSize_, 8);
}

// Serialise a name -> (offset, size) lookup record for `node`'s KD-tree blob
// in "kdtree.bin". Format: [uint8 nameLen][name bytes][uint64 offset][uint64 size].
// Unlike the hierarchy records, this isn't routed through the stepSize
// batch/header files -- every onNodeComplete call appends straight to one
// flat "kdtree_index.bin", since the viewer just loads the whole thing into
// a name->location hashmap once at dataset open and doesn't need BFS order.
void appendKdtreeIndexRecord(std::vector<uint8_t> &out, const std::string &name,
                             uint64_t offset, uint64_t size) {
    auto nameLen = static_cast<uint8_t>(std::min(name.size(), size_t(255)));
    out.push_back(nameLen);
    out.insert(out.end(), name.begin(), name.begin() + nameLen);

    size_t base = out.size();
    out.resize(base + 16);
    std::memcpy(out.data() + base + 0, &offset, 8);
    std::memcpy(out.data() + base + 8, &size,   8);
}

// Buckets nodes into exactly one extra hierarchy batch below the root, at
// depth stepSize — not recursively at every multiple of stepSize. Matches
// PotreeConverter's actual HierarchyFlusher::write (Converter/include/
// indexer.h): its grouping key is always name.substr(0, stepSize+1),
// regardless of how much deeper the node actually is, so an entire subtree
// rooted at depth stepSize lands in one file no matter how deep it goes.
// rebuildIndex() relies on this: it only needs to emit Proxy stubs in the
// root batch (pointing at each depth-stepSize batch file), never inside a
// batch file itself, since nothing below depth stepSize ever lives in a
// separate file.
std::string headerPath(const std::string &nodeName, uint32_t stepSize) {
    uint32_t depth = static_cast<uint32_t>(nodeName.size()) - 1; // "r" => depth 0
    if (depth < stepSize) {
        return "chunks/r.header.bin";
    }
    return "chunks/" + nodeName.substr(0, stepSize + 1) + ".header.bin";
}

} // namespace

// ── Sampler base ──────────────────────────────────────────────────────────────

Sampler::Sampler(std::shared_ptr<ConcurrentWriter> writer,
                 std::string targetDir,
                 Attributes attributes,
                 ConverterOptions options)
    : writer_(std::move(writer)),
      targetDir_(std::move(targetDir)),
      attributes_(std::move(attributes)),
      options_(options) {
    rowStride_ = attributes_.getTotalBytes();
    posOffset_ = attributes_.getOffset("position");
    for (const Attribute &a : attributes_) {
        if (a.name_ == "position") { posAttr_ = a; break; }
    }
    posHandler_ = AttributeHandlerRegistry::get(posAttr_);
}

void Sampler::onNodeComplete(const std::shared_ptr<Node> &node,
                              const std::vector<uint8_t> &accepted) {
    // 1. Write accepted points to octree.bin
    ConcurrentWriter::WriteResult r = writer_->append("octree.bin", accepted);
    node->address_  = r.offset;
    node->byteSize_ = r.size;
    node->numPoints_ = rowStride_ > 0
                        ? static_cast<uint32_t>(accepted.size() / rowStride_)
                        : 0;

    std::vector<vec3f> positions(node->numPoints_);
    for (uint32_t i = 0; i < node->numPoints_; ++i) {
        double pos[3] = {};
        posHandler_->decode(accepted.data() + i * rowStride_ + posOffset_, posAttr_, pos);
        node->tightBB_.expand({pos[0], pos[1], pos[2]});
        positions[i] = vec3f{static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2])};
    }

    // 1b. Build this node's KD-tree from its own final point set (built once,
    // offline, here -- the viewer never rebuilds it, just deserializes the
    // bytes) and write it to "kdtree.bin", recording a name-indexed lookup
    // entry in "kdtree_index.bin".
    if (!positions.empty()) {
        KDTree3 tree;
        tree.build(positions);
        ConcurrentWriter::WriteResult kr = writer_->append("kdtree.bin", tree.serialize());

        std::vector<uint8_t> indexRecord;
        appendKdtreeIndexRecord(indexRecord, node->name_, kr.offset, kr.size);
        writer_->append("kdtree_index.bin", indexRecord);
    }

    // 2. Compute childMask and type
    uint8_t mask = 0;
    for (int c = 0; c < 8; ++c) {
        if (node->children_[c]) mask |= static_cast<uint8_t>(1u << c);
    }
    node->childMask_ = mask;
    node->type_ = (mask == 0) ? NodeType::Leaf : NodeType::Normal;

    // 3. Queue the 22-byte hierarchy record for the appropriate header file
    // (batched and flushed via queueHeaderWrite(), not written immediately).
    std::vector<uint8_t> record;
    appendHierarchyRecord(record, node);
    queueHeaderWrite(headerPath(node->name_, options_.stepSize), std::move(record));
}

void Sampler::queueHeaderWrite(std::string path, std::vector<uint8_t> bytes) {
    std::vector<PendingHeaderWrite> toFlush;
    {
        std::lock_guard<std::mutex> lock(headerMutex_);
        pendingHeaderWrites_.push_back({std::move(path), std::move(bytes)});
        if (pendingHeaderWrites_.size() < kHeaderFlushThreshold) return;
        toFlush = std::move(pendingHeaderWrites_);
        pendingHeaderWrites_.clear();
    }
    flushHeaderWrites(toFlush);
}

void Sampler::flushPendingHeaderWrites() {
    std::vector<PendingHeaderWrite> toFlush;
    {
        std::lock_guard<std::mutex> lock(headerMutex_);
        toFlush = std::move(pendingHeaderWrites_);
        pendingHeaderWrites_.clear();
    }
    flushHeaderWrites(toFlush);
}

void Sampler::flushHeaderWrites(const std::vector<PendingHeaderWrite> &writes) {
    if (writes.empty()) return;

    // Group by target path first so each path is opened/appended/closed once
    // per flush, not once per record.
    std::unordered_map<std::string, std::vector<uint8_t>> grouped;
    for (const PendingHeaderWrite &w : writes) {
        std::vector<uint8_t> &buf = grouped[w.path];
        buf.insert(buf.end(), w.bytes.begin(), w.bytes.end());
    }

    for (auto &[path, buf] : grouped) {
        writer_->appendAndClose(path, buf);
    }
}

// ── RandomSampler ─────────────────────────────────────────────────────────────

void RandomSampler::doSample(const PointBatch &candidates,
                              const std::shared_ptr<Node> & /*node*/,
                              double spacing,
                              std::vector<uint8_t> &accepted,
                              std::vector<uint8_t> &rejected,
                              std::vector<uint8_t> &acceptFlags) {
    accepted.clear();
    rejected.clear();
    acceptFlags.assign(candidates.numPoints, 0);
    if (candidates.numPoints == 0 || candidates.rowStride == 0) return;

    // Keep every other point (stride 2). Adjust stride by spacing if needed.
    uint64_t stride = (spacing > 0.0) ? std::max<uint64_t>(2, 1) : 1;
    for (uint64_t i = 0; i < candidates.numPoints; ++i) {
        const uint8_t *row = candidates.data + i * candidates.rowStride;
        if (i % stride == 0) {
            accepted.insert(accepted.end(), row, row + candidates.rowStride);
            acceptFlags[i] = 1;
        } else {
            rejected.insert(rejected.end(), row, row + candidates.rowStride);
        }
    }
}

// ── PoissonDiskSampler ────────────────────────────────────────────────────────

void PoissonDiskSampler::doSample(const PointBatch &candidates,
                                   const std::shared_ptr<Node> &node,
                                   double spacing,
                                   std::vector<uint8_t> &accepted,
                                   std::vector<uint8_t> &rejected,
                                   std::vector<uint8_t> &acceptFlags) {
    accepted.clear();
    rejected.clear();
    const uint64_t n = candidates.numPoints;
    acceptFlags.assign(n, 0);
    if (n == 0) return;

    if (spacing <= 0.0 || !posHandler_) {
        accepted.assign(candidates.data, candidates.data + n * candidates.rowStride);
        acceptFlags.assign(n, 1);
        return;
    }

    struct Point { double x, y, z; uint64_t idx; };

    // Decode all positions.
    std::vector<Point> points(n);
    for (uint64_t i = 0; i < n; ++i) {
        double pos[3] = {};
        posHandler_->decode(candidates.data + i * candidates.rowStride + posOffset_, posAttr_, pos);
        points[i] = {pos[0], pos[1], pos[2], i};
    }

    // Sort candidates by distance to node center (closest first).
    // Accepted points are appended in this order, so acceptedBuf stays sorted
    // by center-distance — enabling the early-stop below.
    const vec3d bbMin = node->bb_.min();
    const vec3d bbMax = node->bb_.max();
    const double cx = (bbMin.x + bbMax.x) * 0.5;
    const double cy = (bbMin.y + bbMax.y) * 0.5;
    const double cz = (bbMin.z + bbMax.z) * 0.5;

    std::sort(points.begin(), points.end(), [cx, cy, cz](const Point &a, const Point &b) {
        double adx = a.x - cx, ady = a.y - cy, adz = a.z - cz;
        double bdx = b.x - cx, bdy = b.y - cy, bdz = b.z - cz;
        return (adx*adx + ady*ady + adz*adz) < (bdx*bdx + bdy*bdy + bdz*bdz);
    });

    // Greedy accept/reject: each thread keeps its own accepted buffer so
    // doSample stays stateless across concurrent calls.
    thread_local std::vector<Point> acceptedBuf;
    acceptedBuf.clear();
    if (acceptedBuf.capacity() < n) acceptedBuf.reserve(n);

    const double spacingSq = spacing * spacing;
    std::vector<bool> flags(n, false);

    for (const Point &cand : points) {
        double dx = cand.x - cx, dy = cand.y - cy, dz = cand.z - cz;
        double candDistSq = dx*dx + dy*dy + dz*dz;
        double candDist   = std::sqrt(candDistSq);

        // An accepted point closer to center than (candDist - spacing) is
        // guaranteed to be at least `spacing` away from cand (reverse triangle
        // inequality). Since acceptedBuf is sorted closest-first, once we hit
        // such a point while iterating backwards we can stop.
        double limit   = candDist - spacing;
        double limitSq = limit * limit;

        bool accept = true;
        int64_t checks = 0;
        for (int64_t i = static_cast<int64_t>(acceptedBuf.size()) - 1; i >= 0; --i) {
            const Point &p = acceptedBuf[i];

            double pdx = p.x - cx, pdy = p.y - cy, pdz = p.z - cz;
            double pDistSq = pdx*pdx + pdy*pdy + pdz*pdz;
            if (pDistSq < limitSq) break;  // no closer accepted point can conflict

            double ddx = cand.x - p.x, ddy = cand.y - p.y, ddz = cand.z - p.z;
            if (ddx*ddx + ddy*ddy + ddz*ddz < spacingSq) { accept = false; break; }

            if (++checks > 10'000) break;
        }

        flags[cand.idx] = accept;
        if (accept) acceptedBuf.push_back(cand);
    }

    // Single-pass bulk copy into output vectors.
    const uint64_t stride = candidates.rowStride;
    for (uint64_t i = 0; i < n; ++i) {
        const uint8_t *row = candidates.data + i * stride;
        if (flags[i]) {
            accepted.insert(accepted.end(), row, row + stride);
            acceptFlags[i] = 1;
        } else {
            rejected.insert(rejected.end(), row, row + stride);
        }
    }
}

// ── PoissonAverageSampler ────────────────────────────────────────────────────

void PoissonAverageSampler::doSample(const PointBatch &candidates,
                                      const std::shared_ptr<Node> &node,
                                      double spacing,
                                      std::vector<uint8_t> &accepted,
                                      std::vector<uint8_t> &rejected,
                                      std::vector<uint8_t> &acceptFlags) {
    accepted.clear();
    rejected.clear();
    acceptFlags.assign(candidates.numPoints, 0);
    if (candidates.numPoints == 0) return;

    if (spacing <= 0.0) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
        acceptFlags.assign(candidates.numPoints, 1);
        return;
    }

    if (!posHandler_) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
        acceptFlags.assign(candidates.numPoints, 1);
        return;
    }

    const uint64_t n = candidates.numPoints;
    const uint64_t stride = candidates.rowStride;
    const double spacingSq = spacing * spacing;

    // Decode positions and compute Morton codes for spatial ordering of the
    // accepted set (enables faster neighbourhood lookup).
    std::vector<vec3d> positions(n);
    std::vector<uint64_t> codes(n);
    const BoundingBoxd &bb = node->bb_;

    for (uint64_t i = 0; i < n; ++i) {
        double pos[3] = {};
        posHandler_->decode(candidates.data + i * stride + posOffset_, posAttr_, pos);
        positions[i] = {pos[0], pos[1], pos[2]};
        codes[i] = octree_naming::mortonOf(positions[i], bb);
    }

    // Accepted set: sorted by Morton code so neighbours cluster together.
    // Each entry stores (morton code, world position).
    struct AcceptedEntry { uint64_t code; vec3d pos; };
    std::vector<AcceptedEntry> acceptedSet;
    acceptedSet.reserve(n / 2);

    // Flag array: true = accepted.
    std::vector<bool> flags(n, false);

    for (uint64_t i = 0; i < n; ++i) {
        const vec3d &p = positions[i];
        bool tooClose = false;

        // Search the sorted accepted set for nearby points. Because Morton
        // codes have spatial locality, nearby entries in acceptedSet are
        // likely spatial neighbours. A full scan is safe; a sorted binary
        // search + window can optimise hot paths later.
        for (const AcceptedEntry &e : acceptedSet) {
            double dx = p.x - e.pos.x, dy = p.y - e.pos.y, dz = p.z - e.pos.z;
            if (dx*dx + dy*dy + dz*dz < spacingSq) { tooClose = true; break; }
        }

        if (!tooClose) {
            flags[i] = true;
            // Insert into sorted position (keep acceptedSet sorted by code).
            AcceptedEntry entry{codes[i], p};
            auto it = std::lower_bound(acceptedSet.begin(), acceptedSet.end(), entry,
                                        [](const AcceptedEntry &a, const AcceptedEntry &b) {
                                            return a.code < b.code;
                                        });
            acceptedSet.insert(it, entry);
        }
    }

    // Bulk copy: one pass over the flag array.
    for (uint64_t i = 0; i < n; ++i) {
        const uint8_t *row = candidates.data + i * stride;
        if (flags[i]) {
            accepted.insert(accepted.end(), row, row + stride);
            acceptFlags[i] = 1;
        } else {
            rejected.insert(rejected.end(), row, row + stride);
        }
    }
}

// ── Factory ───────────────────────────────────────────────────────────────────

std::unique_ptr<Sampler> createSampler(const std::string &name,
                                        std::shared_ptr<ConcurrentWriter> writer,
                                        std::string targetDir,
                                        Attributes attributes,
                                        ConverterOptions options) {
    if (name == "random")
        return std::make_unique<RandomSampler>(writer, targetDir, attributes, options);
    if (name == "poisson_average")
        return std::make_unique<PoissonAverageSampler>(writer, targetDir, attributes, options);
    return std::make_unique<PoissonDiskSampler>(writer, std::move(targetDir),
                                                 std::move(attributes), options);
}
