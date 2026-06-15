//
// Created by cj on 2026-06-14.
//

#include "sampler.h"

#include <cmath>
#include <unordered_map>

#include "attribute_handler/attribute_handler_registry.h"

namespace {

struct CellKey {
    int64_t x, y, z;

    bool operator==(const CellKey &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
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

void PoissonDiskSampler::sample(const PointBatch &candidates, const BoundingBoxd &nodeBB, double spacing,
                                 const Attributes &attributes,
                                 std::vector<uint8_t> &accepted, std::vector<uint8_t> &rejected) const {
    accepted.clear();
    rejected.clear();

    if (candidates.numPoints == 0) return;

    // A non-positive spacing means "keep everything" (e.g. spacing underflowed
    // to 0 at very deep levels).
    if (spacing <= 0.0) {
        accepted.assign(candidates.data, candidates.data + candidates.numPoints * candidates.rowStride);
        return;
    }

    const Attribute *positionAttr = nullptr;
    for (const Attribute &attr : attributes) {
        if (attr.name_ == "position") {
            positionAttr = &attr;
            break;
        }
    }
    if (!positionAttr) {
        // No position attribute to sample on: keep everything.
        accepted.assign(candidates.data, candidates.data + candidates.numPoints * candidates.rowStride);
        return;
    }

    const uint64_t positionOffset = attributes.getOffset("position");
    const AttributeHandler *positionHandler = AttributeHandlerRegistry::get(*positionAttr);

    const vec3d origin = nodeBB.min();
    const double spacingSq = spacing * spacing;

    std::unordered_map<CellKey, std::vector<vec3d>, CellKeyHash> grid;

    for (uint64_t i = 0; i < candidates.numPoints; ++i) {
        const uint8_t *row = candidates.data + i * candidates.rowStride;

        double posOut[3] = {0, 0, 0};
        positionHandler->decode(row + positionOffset, *positionAttr, posOut);
        vec3d p{posOut[0], posOut[1], posOut[2]};

        CellKey key = cellKeyOf(p, origin, spacing);

        bool tooClose = false;
        for (int dx = -1; dx <= 1 && !tooClose; ++dx) {
            for (int dy = -1; dy <= 1 && !tooClose; ++dy) {
                for (int dz = -1; dz <= 1 && !tooClose; ++dz) {
                    auto it = grid.find(CellKey{key.x + dx, key.y + dy, key.z + dz});
                    if (it == grid.end()) continue;

                    for (const vec3d &q : it->second) {
                        double ddx = p.x - q.x;
                        double ddy = p.y - q.y;
                        double ddz = p.z - q.z;
                        if (ddx * ddx + ddy * ddy + ddz * ddz < spacingSq) {
                            tooClose = true;
                            break;
                        }
                    }
                }
            }
        }

        if (tooClose) {
            rejected.insert(rejected.end(), row, row + candidates.rowStride);
        } else {
            grid[key].push_back(p);
            accepted.insert(accepted.end(), row, row + candidates.rowStride);
        }
    }
}

std::unique_ptr<Sampler> createSampler(const std::string &name) {
    (void)name;
    return std::make_unique<PoissonDiskSampler>();
}
