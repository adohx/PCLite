//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_OCTREE_NAMING_H
#define PCLITE_OCTREE_NAMING_H

#include <array>
#include <cstdint>
#include <string>

#include "bounding_box.h"
#include "vec3.h"

// Shared Potree-style octree geometry/naming helpers used by Chunker,
// HierarchyBuilder and Indexer, so that node names, grid cell indices and
// bounding-box subdivisions all agree on the same x/y/z <-> octant mapping
// (digit = 4*X + 2*Y + Z, X/Y/Z in {0,1} selecting the upper/lower half of
// the parent cube along each axis).
namespace octree_naming {

// "r" for the root (level 0), followed by one octal digit per level
// describing which octant of the parent cube (x,y,z) falls into.
std::string toNodeID(int level, uint32_t gridSizeAtLevel, uint32_t x, uint32_t y, uint32_t z);

// The 8 cell indices into a (2*sizeLow)^3 grid that subdivide cell (x,y,z) of
// a sizeLow^3 grid. Entry j corresponds to digit j = 4*X + 2*Y + Z.
std::array<uint64_t, 8> subdividedIndices(uint32_t x, uint32_t y, uint32_t z, uint32_t sizeLow);

// Flat index (x + y*gridSize + z*gridSize^2) of the finest-grid cell
// containing `p`, for a gridSize^3 grid covering `aabb`.
uint64_t cellIndex(const vec3d &p, const BoundingBoxd &aabb, uint32_t gridSize);

// Child `childIndex` (digit = 4*X + 2*Y + Z) of `parentBB`, bisecting each axis.
BoundingBoxd childBoundingBox(const BoundingBoxd &parentBB, int childIndex);

// Inverse of childBoundingBox: the child digit (4*X + 2*Y + Z) of `parentBB`
// that contains `p`, based on which half of each axis `p` falls into.
int childIndexOf(const vec3d &p, const BoundingBoxd &parentBB);

// Interleave three 21-bit integers into a 63-bit Morton (Z-order) code.
// Bit layout (MSB first): X20 Y20 Z20 X19 Y19 Z19 ... X0 Y0 Z0
// so that the top 3 bits give child digit 4*X+2*Y+Z at the root level,
// matching the 4*X+2*Y+Z octant convention used by toNodeID/childBoundingBox.
uint64_t mortonEncode(uint32_t ix, uint32_t iy, uint32_t iz);

// Compute the Morton code of world-space point p within aabb, using 21 bits
// per axis (the finest grid that fits in a 63-bit code).
uint64_t mortonOf(const vec3d &p, const BoundingBoxd &aabb);

// Extract the child digit (0-7) at octree level `level` (0 = root) from a
// Morton code produced by mortonEncode/mortonOf.
inline int mortonChildAt(uint64_t code, int level) {
    return static_cast<int>((code >> (60 - 3 * level)) & 7u);
}

} // namespace octree_naming

#endif //PCLITE_OCTREE_NAMING_H
