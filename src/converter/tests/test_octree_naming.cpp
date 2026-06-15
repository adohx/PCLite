//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include "bounding_box.h"
#include "octree_naming.h"

using octree_naming::cellIndex;
using octree_naming::childBoundingBox;
using octree_naming::childIndexOf;
using octree_naming::subdividedIndices;
using octree_naming::toNodeID;

TEST(OctreeNaming, ToNodeIDRoot) {
    EXPECT_EQ(toNodeID(0, 1, 0, 0, 0), "r");
}

TEST(OctreeNaming, ToNodeIDOneLevelCoversAllOctants) {
    EXPECT_EQ(toNodeID(1, 2, 0, 0, 0), "r0");
    EXPECT_EQ(toNodeID(1, 2, 1, 0, 0), "r4");
    EXPECT_EQ(toNodeID(1, 2, 0, 1, 0), "r2");
    EXPECT_EQ(toNodeID(1, 2, 0, 0, 1), "r1");
    EXPECT_EQ(toNodeID(1, 2, 1, 1, 1), "r7");
    EXPECT_EQ(toNodeID(1, 2, 1, 0, 1), "r5");
    EXPECT_EQ(toNodeID(1, 2, 0, 1, 1), "r3");
    EXPECT_EQ(toNodeID(1, 2, 1, 1, 0), "r6");
}

TEST(OctreeNaming, ToNodeIDMultiLevel) {
    // x=3 (binary 11), y=1 (binary 01), z=2 (binary 10) at level 2 (gridSizeAtLevel=4):
    // digit0 = 4*x_msb + 2*y_msb + z_msb = 4*1 + 2*0 + 1 = 5
    // digit1 = 4*x_lsb + 2*y_lsb + z_lsb = 4*1 + 2*1 + 0 = 6
    EXPECT_EQ(toNodeID(2, 4, 3, 1, 2), "r56");
}

TEST(OctreeNaming, SubdividedIndicesOfRootCell) {
    auto result = subdividedIndices(0, 0, 0, 1);
    std::array<uint64_t, 8> expected = {0, 4, 2, 6, 1, 5, 3, 7};
    EXPECT_EQ(result, expected);
}

TEST(OctreeNaming, SubdividedIndicesOfNonOriginCell) {
    // (x,y,z) = (1,0,1) within a 2^3 grid subdivides into a 4^3 grid.
    auto result = subdividedIndices(1, 0, 1, 2);
    std::array<uint64_t, 8> expected = {34, 50, 38, 54, 35, 51, 39, 55};
    EXPECT_EQ(result, expected);
}

TEST(OctreeNaming, CellIndexInteriorAndCorners) {
    BoundingBoxd aabb({0, 0, 0}, {8, 8, 8});

    EXPECT_EQ(cellIndex({0.5, 0.5, 0.5}, aabb, 8), 0u);
    EXPECT_EQ(cellIndex({4.0, 0.0, 0.0}, aabb, 8), 4u);
    EXPECT_EQ(cellIndex({7.99, 7.99, 7.99}, aabb, 8), 511u);
}

TEST(OctreeNaming, CellIndexClampsOutOfRangePoints) {
    BoundingBoxd aabb({0, 0, 0}, {8, 8, 8});

    // Exactly on the max corner falls outside [min,max) and is clamped back in.
    EXPECT_EQ(cellIndex({8.0, 8.0, 8.0}, aabb, 8), 511u);

    // Below the min corner clamps to cell 0.
    EXPECT_EQ(cellIndex({-1.0, -1.0, -1.0}, aabb, 8), 0u);
}

TEST(OctreeNaming, ChildBoundingBoxBisectsEachAxis) {
    BoundingBoxd parent({0, 0, 0}, {8, 8, 8});

    auto c0 = childBoundingBox(parent, 0);
    EXPECT_DOUBLE_EQ(c0.min().x, 0); EXPECT_DOUBLE_EQ(c0.min().y, 0); EXPECT_DOUBLE_EQ(c0.min().z, 0);
    EXPECT_DOUBLE_EQ(c0.max().x, 4); EXPECT_DOUBLE_EQ(c0.max().y, 4); EXPECT_DOUBLE_EQ(c0.max().z, 4);

    auto c7 = childBoundingBox(parent, 7);
    EXPECT_DOUBLE_EQ(c7.min().x, 4); EXPECT_DOUBLE_EQ(c7.min().y, 4); EXPECT_DOUBLE_EQ(c7.min().z, 4);
    EXPECT_DOUBLE_EQ(c7.max().x, 8); EXPECT_DOUBLE_EQ(c7.max().y, 8); EXPECT_DOUBLE_EQ(c7.max().z, 8);

    auto c4 = childBoundingBox(parent, 4); // X=1,Y=0,Z=0
    EXPECT_DOUBLE_EQ(c4.min().x, 4); EXPECT_DOUBLE_EQ(c4.min().y, 0); EXPECT_DOUBLE_EQ(c4.min().z, 0);
    EXPECT_DOUBLE_EQ(c4.max().x, 8); EXPECT_DOUBLE_EQ(c4.max().y, 4); EXPECT_DOUBLE_EQ(c4.max().z, 4);

    auto c1 = childBoundingBox(parent, 1); // X=0,Y=0,Z=1
    EXPECT_DOUBLE_EQ(c1.min().x, 0); EXPECT_DOUBLE_EQ(c1.min().y, 0); EXPECT_DOUBLE_EQ(c1.min().z, 4);
    EXPECT_DOUBLE_EQ(c1.max().x, 4); EXPECT_DOUBLE_EQ(c1.max().y, 4); EXPECT_DOUBLE_EQ(c1.max().z, 8);
}

TEST(OctreeNaming, ChildIndexOfIsInverseOfChildBoundingBox) {
    BoundingBoxd parent({0, 0, 0}, {8, 8, 8});

    for (int childIndex = 0; childIndex < 8; ++childIndex) {
        BoundingBoxd child = childBoundingBox(parent, childIndex);
        vec3d size = child.getSize();
        // A point at the child's own center must map back to childIndex.
        vec3d center = {
            child.min().x + size.x / 2.0,
            child.min().y + size.y / 2.0,
            child.min().z + size.z / 2.0,
        };
        EXPECT_EQ(childIndexOf(center, parent), childIndex);
    }
}

TEST(OctreeNaming, ChildIndexOfBoundaryPointsFallIntoUpperHalf) {
    BoundingBoxd parent({0, 0, 0}, {8, 8, 8});

    // Exactly on the midpoint along every axis: counts as the upper half
    // (X=1,Y=1,Z=1 => digit 7), matching childBoundingBox(parent,7)'s min corner.
    EXPECT_EQ(childIndexOf({4, 4, 4}, parent), 7);

    // Lower-half corner.
    EXPECT_EQ(childIndexOf({0, 0, 0}, parent), 0);
}
