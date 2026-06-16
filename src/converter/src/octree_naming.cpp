//
// Created by cj on 2026-06-14.
//

#include "octree_naming.h"

#include <algorithm>
#include <cmath>

namespace octree_naming {

std::string toNodeID(int level, uint32_t gridSizeAtLevel, uint32_t x, uint32_t y, uint32_t z) {
    std::string id = "r";
    uint32_t cur = gridSizeAtLevel;

    for (int i = 0; i < level; ++i) {
        uint32_t half = cur / 2;
        int digit = 0;
        if (x >= half) { digit += 4; x -= half; }
        if (y >= half) { digit += 2; y -= half; }
        if (z >= half) { digit += 1; z -= half; }
        id += std::to_string(digit);
        cur = half;
    }

    return id;
}

std::array<uint64_t, 8> subdividedIndices(uint32_t x, uint32_t y, uint32_t z, uint32_t sizeLow) {
    uint64_t sizeHigh = static_cast<uint64_t>(sizeLow) * 2;

    std::array<uint64_t, 8> result{};
    for (int j = 0; j < 8; ++j) {
        uint32_t ox = (j >> 2) & 1;
        uint32_t oy = (j >> 1) & 1;
        uint32_t oz = j & 1;

        uint64_t nx = static_cast<uint64_t>(x) * 2 + ox;
        uint64_t ny = static_cast<uint64_t>(y) * 2 + oy;
        uint64_t nz = static_cast<uint64_t>(z) * 2 + oz;

        result[j] = nx + ny * sizeHigh + nz * sizeHigh * sizeHigh;
    }
    return result;
}

uint64_t cellIndex(const vec3d &p, const BoundingBoxd &aabb, uint32_t gridSize) {
    vec3d lo = aabb.min();
    vec3d size = aabb.getSize();

    auto normalize = [](double value, double origin, double extent) -> double {
        return extent != 0.0 ? (value - origin) / extent : 0.0;
    };

    double ux = normalize(p.x, lo.x, size.x);
    double uy = normalize(p.y, lo.y, size.y);
    double uz = normalize(p.z, lo.z, size.z);

    auto toCell = [&](double u) -> uint64_t {
        int64_t i = static_cast<int64_t>(std::floor(u * gridSize));
        return static_cast<uint64_t>(std::clamp<int64_t>(i, 0, static_cast<int64_t>(gridSize) - 1));
    };

    uint64_t ix = toCell(ux);
    uint64_t iy = toCell(uy);
    uint64_t iz = toCell(uz);

    return ix + iy * gridSize + iz * static_cast<uint64_t>(gridSize) * gridSize;
}

BoundingBoxd childBoundingBox(const BoundingBoxd &parentBB, int childIndex) {
    vec3d pmin = parentBB.min();
    vec3d pmax = parentBB.max();
    vec3d mid = {(pmin.x + pmax.x) / 2.0, (pmin.y + pmax.y) / 2.0, (pmin.z + pmax.z) / 2.0};

    int X = (childIndex >> 2) & 1;
    int Y = (childIndex >> 1) & 1;
    int Z = childIndex & 1;

    vec3d cmin = {X ? mid.x : pmin.x, Y ? mid.y : pmin.y, Z ? mid.z : pmin.z};
    vec3d cmax = {X ? pmax.x : mid.x, Y ? pmax.y : mid.y, Z ? pmax.z : mid.z};

    return BoundingBoxd(cmin, cmax);
}

int childIndexOf(const vec3d &p, const BoundingBoxd &parentBB) {
    vec3d pmin = parentBB.min();
    vec3d pmax = parentBB.max();
    vec3d mid = {(pmin.x + pmax.x) / 2.0, (pmin.y + pmax.y) / 2.0, (pmin.z + pmax.z) / 2.0};

    int X = p.x >= mid.x ? 1 : 0;
    int Y = p.y >= mid.y ? 1 : 0;
    int Z = p.z >= mid.z ? 1 : 0;

    return 4 * X + 2 * Y + Z;
}

uint64_t mortonEncode(uint32_t ix, uint32_t iy, uint32_t iz) {
    uint64_t code = 0;
    for (int i = 0; i < 21; ++i) {
        int shift = 60 - 3 * i;
        code |= static_cast<uint64_t>((ix >> (20 - i)) & 1u) << (shift + 2);
        code |= static_cast<uint64_t>((iy >> (20 - i)) & 1u) << (shift + 1);
        code |= static_cast<uint64_t>((iz >> (20 - i)) & 1u) << shift;
    }
    return code;
}

uint64_t mortonOf(const vec3d &p, const BoundingBoxd &aabb) {
    constexpr int kDepth = 21;
    constexpr uint32_t kGrid = 1u << kDepth;

    vec3d lo = aabb.min();
    vec3d sz = aabb.getSize();

    auto toInt = [&](double v, double origin, double extent) -> uint32_t {
        double u = extent != 0.0 ? (v - origin) / extent : 0.0;
        int64_t i = static_cast<int64_t>(std::floor(u * kGrid));
        return static_cast<uint32_t>(std::clamp<int64_t>(i, 0, kGrid - 1));
    };

    return mortonEncode(toInt(p.x, lo.x, sz.x), toInt(p.y, lo.y, sz.y), toInt(p.z, lo.z, sz.z));
}

} // namespace octree_naming
