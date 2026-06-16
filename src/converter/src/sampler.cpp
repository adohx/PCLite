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
#include "octree_naming.h"

namespace {

constexpr size_t kHierarchyRecordBytes = 22;

// Serialise one 22-byte hierarchy record for `node` into `out`.
void appendHierarchyRecord(std::vector<uint8_t> &out, const std::shared_ptr<Node> &node) {
    size_t base = out.size();
    out.resize(base + kHierarchyRecordBytes);
    out[base + 0] = static_cast<uint8_t>(node->type_);
    out[base + 1] = node->childMask_;
    std::memcpy(out.data() + base + 2, &node->numPoints_, 4);
    std::memcpy(out.data() + base + 6, &node->address_,  8);
    std::memcpy(out.data() + base + 14, &node->byteSize_, 8);
}

// Return "chunks/r.header.bin" for nodes in the root group (name length <
// stepSize+1), or "chunks/<batchName>.header.bin" for batch-group nodes.
std::string headerPath(const std::string &nodeName, uint32_t stepSize) {
    if (static_cast<uint32_t>(nodeName.size()) < stepSize + 1) {
        return "chunks/r.header.bin";
    }
    return "chunks/" + nodeName.substr(0, stepSize + 1) + ".header.bin";
}

struct CellKey {
    int64_t x, y, z;
    bool operator==(const CellKey &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct CellKeyHash {
    size_t operator()(const CellKey &k) const {
        size_t h = std::hash<int64_t>()(k.x);
        h ^= std::hash<int64_t>()(k.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<int64_t>()(k.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

CellKey cellKeyOf(const vec3d &p, const vec3d &origin, double spacing) {
    return {
        static_cast<int64_t>(std::floor((p.x - origin.x) / spacing)),
        static_cast<int64_t>(std::floor((p.y - origin.y) / spacing)),
        static_cast<int64_t>(std::floor((p.z - origin.z) / spacing)),
    };
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

    for (uint32_t i = 0; i < node->numPoints_; ++i) {
        double pos[3] = {};
        posHandler_->decode(accepted.data() + i * rowStride_ + posOffset_, posAttr_, pos);
        node->tightBB_.expand({pos[0], pos[1], pos[2]});
    }

    // 2. Compute childMask and type
    uint8_t mask = 0;
    for (int c = 0; c < 8; ++c) {
        if (node->children_[c]) mask |= static_cast<uint8_t>(1u << c);
    }
    node->childMask_ = mask;
    node->type_ = (mask == 0) ? NodeType::Leaf : NodeType::Normal;

    // 3. Write 22-byte hierarchy record to the appropriate header file
    std::vector<uint8_t> record;
    appendHierarchyRecord(record, node);
    writer_->append(headerPath(node->name_, options_.stepSize), record);
}

// ── RandomSampler ─────────────────────────────────────────────────────────────

void RandomSampler::doSample(const PointBatch &candidates,
                              const std::shared_ptr<Node> & /*node*/,
                              double spacing,
                              std::vector<uint8_t> &accepted,
                              std::vector<uint8_t> &rejected) {
    accepted.clear();
    rejected.clear();
    if (candidates.numPoints == 0 || candidates.rowStride == 0) return;

    // Keep every other point (stride 2). Adjust stride by spacing if needed.
    uint64_t stride = (spacing > 0.0) ? std::max<uint64_t>(2, 1) : 1;
    for (uint64_t i = 0; i < candidates.numPoints; ++i) {
        const uint8_t *row = candidates.data + i * candidates.rowStride;
        if (i % stride == 0)
            accepted.insert(accepted.end(), row, row + candidates.rowStride);
        else
            rejected.insert(rejected.end(), row, row + candidates.rowStride);
    }
}

// ── PoissonDiskSampler ────────────────────────────────────────────────────────

void PoissonDiskSampler::doSample(const PointBatch &candidates,
                                   const std::shared_ptr<Node> & /*node*/,
                                   double spacing,
                                   std::vector<uint8_t> &accepted,
                                   std::vector<uint8_t> &rejected) {
    accepted.clear();
    rejected.clear();
    if (candidates.numPoints == 0) return;

    if (spacing <= 0.0) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
        return;
    }

    if (!posHandler_) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
        return;
    }

    const double spacingSq = spacing * spacing;
    std::unordered_map<CellKey, std::vector<vec3d>, CellKeyHash> grid;

    for (uint64_t i = 0; i < candidates.numPoints; ++i) {
        const uint8_t *row = candidates.data + i * candidates.rowStride;
        double pos[3] = {};
        posHandler_->decode(row + posOffset_, posAttr_, pos);
        vec3d p{pos[0], pos[1], pos[2]};

        CellKey key = cellKeyOf(p, {0, 0, 0}, spacing);

        bool tooClose = false;
        for (int dx = -1; dx <= 1 && !tooClose; ++dx)
            for (int dy = -1; dy <= 1 && !tooClose; ++dy)
                for (int dz = -1; dz <= 1 && !tooClose; ++dz) {
                    auto it = grid.find({key.x + dx, key.y + dy, key.z + dz});
                    if (it == grid.end()) continue;
                    for (const vec3d &q : it->second) {
                        double dx2 = p.x - q.x, dy2 = p.y - q.y, dz2 = p.z - q.z;
                        if (dx2*dx2 + dy2*dy2 + dz2*dz2 < spacingSq) { tooClose = true; break; }
                    }
                }

        if (tooClose)
            rejected.insert(rejected.end(), row, row + candidates.rowStride);
        else {
            grid[key].push_back(p);
            accepted.insert(accepted.end(), row, row + candidates.rowStride);
        }
    }
}

// ── PoissonAverageSampler ────────────────────────────────────────────────────

void PoissonAverageSampler::doSample(const PointBatch &candidates,
                                      const std::shared_ptr<Node> &node,
                                      double spacing,
                                      std::vector<uint8_t> &accepted,
                                      std::vector<uint8_t> &rejected) {
    accepted.clear();
    rejected.clear();
    if (candidates.numPoints == 0) return;

    if (spacing <= 0.0) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
        return;
    }

    if (!posHandler_) {
        accepted.assign(candidates.data,
                        candidates.data + candidates.numPoints * candidates.rowStride);
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
        if (flags[i])
            accepted.insert(accepted.end(), row, row + stride);
        else
            rejected.insert(rejected.end(), row, row + stride);
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
