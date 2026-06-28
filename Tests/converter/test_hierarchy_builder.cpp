//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include "hierarchy_builder.h"

namespace {

ChunkInfo makeChunk(const std::string &name, uint64_t numPoints) {
    ChunkInfo c;
    c.name = name;
    c.numPoints = numPoints;
    return c;
}

} // namespace

TEST(HierarchyBuilder, BuildsTreeShapeFromChunkPaths) {
    BoundingBoxd rootAABB({0, 0, 0}, {8, 8, 8});
    HierarchyBuilder builder(rootAABB);

    std::vector<ChunkInfo> chunks = {
        makeChunk("r", 100),
        makeChunk("r03", 50),
        makeChunk("r047", 25),
    };

    std::vector<std::shared_ptr<Node>> chunkRoots;
    std::shared_ptr<Node> root = builder.build(chunks, chunkRoots);

    ASSERT_EQ(chunkRoots.size(), 3u);

    // chunks[0] = "r" is the root itself.
    EXPECT_EQ(root->name_, "r");
    EXPECT_EQ(root->level_, 0);
    EXPECT_EQ(root->numPoints_, 100u);
    EXPECT_EQ(chunkRoots[0], root);
    EXPECT_DOUBLE_EQ(root->bb_.min().x, 0.0);
    EXPECT_DOUBLE_EQ(root->bb_.max().x, 8.0);

    // chunks[1] = "r03" creates intermediate node "r0" plus leaf "r03".
    std::shared_ptr<Node> r0 = root->children_[0];
    ASSERT_NE(r0, nullptr);
    EXPECT_EQ(r0->name_, "r0");
    EXPECT_EQ(r0->level_, 1);
    EXPECT_EQ(r0->parent_, root);
    EXPECT_DOUBLE_EQ(r0->bb_.min().x, 0.0); EXPECT_DOUBLE_EQ(r0->bb_.min().y, 0.0); EXPECT_DOUBLE_EQ(r0->bb_.min().z, 0.0);
    EXPECT_DOUBLE_EQ(r0->bb_.max().x, 4.0); EXPECT_DOUBLE_EQ(r0->bb_.max().y, 4.0); EXPECT_DOUBLE_EQ(r0->bb_.max().z, 4.0);

    std::shared_ptr<Node> r03 = r0->children_[3];
    ASSERT_NE(r03, nullptr);
    EXPECT_EQ(r03, chunkRoots[1]);
    EXPECT_EQ(r03->name_, "r03");
    EXPECT_EQ(r03->level_, 2);
    EXPECT_EQ(r03->parent_, r0);
    EXPECT_EQ(r03->numPoints_, 50u);
    // childIndex 3 = (X=0,Y=1,Z=1): upper Y/Z half, lower X half of r0's bb.
    EXPECT_DOUBLE_EQ(r03->bb_.min().x, 0.0); EXPECT_DOUBLE_EQ(r03->bb_.min().y, 2.0); EXPECT_DOUBLE_EQ(r03->bb_.min().z, 2.0);
    EXPECT_DOUBLE_EQ(r03->bb_.max().x, 2.0); EXPECT_DOUBLE_EQ(r03->bb_.max().y, 4.0); EXPECT_DOUBLE_EQ(r03->bb_.max().z, 4.0);

    // chunks[2] = "r047" creates intermediate node "r04" (sharing the
    // already-created "r0") plus leaf "r047".
    std::shared_ptr<Node> r04 = r0->children_[4];
    ASSERT_NE(r04, nullptr);
    EXPECT_EQ(r04->name_, "r04");
    EXPECT_EQ(r04->level_, 2);
    // childIndex 4 = (X=1,Y=0,Z=0): upper X half, lower Y/Z half of r0's bb.
    EXPECT_DOUBLE_EQ(r04->bb_.min().x, 2.0); EXPECT_DOUBLE_EQ(r04->bb_.min().y, 0.0); EXPECT_DOUBLE_EQ(r04->bb_.min().z, 0.0);
    EXPECT_DOUBLE_EQ(r04->bb_.max().x, 4.0); EXPECT_DOUBLE_EQ(r04->bb_.max().y, 2.0); EXPECT_DOUBLE_EQ(r04->bb_.max().z, 2.0);

    std::shared_ptr<Node> r047 = r04->children_[7];
    ASSERT_NE(r047, nullptr);
    EXPECT_EQ(r047, chunkRoots[2]);
    EXPECT_EQ(r047->name_, "r047");
    EXPECT_EQ(r047->level_, 3);
    EXPECT_EQ(r047->parent_, r04);
    EXPECT_EQ(r047->numPoints_, 25u);
    // childIndex 7 = (X=1,Y=1,Z=1): upper half on every axis of r04's bb.
    EXPECT_DOUBLE_EQ(r047->bb_.min().x, 3.0); EXPECT_DOUBLE_EQ(r047->bb_.min().y, 1.0); EXPECT_DOUBLE_EQ(r047->bb_.min().z, 1.0);
    EXPECT_DOUBLE_EQ(r047->bb_.max().x, 4.0); EXPECT_DOUBLE_EQ(r047->bb_.max().y, 2.0); EXPECT_DOUBLE_EQ(r047->bb_.max().z, 2.0);
}

TEST(HierarchyBuilder, SharedPrefixesReuseTheSameIntermediateNode) {
    BoundingBoxd rootAABB({0, 0, 0}, {1, 1, 1});
    HierarchyBuilder builder(rootAABB);

    std::vector<ChunkInfo> chunks = {
        makeChunk("r07", 10),
        makeChunk("r071", 20),
    };

    std::vector<std::shared_ptr<Node>> chunkRoots;
    std::shared_ptr<Node> root = builder.build(chunks, chunkRoots);

    ASSERT_EQ(chunkRoots.size(), 2u);
    EXPECT_EQ(chunkRoots[1]->parent_, chunkRoots[0]);
    EXPECT_EQ(chunkRoots[0]->children_[1], chunkRoots[1]);
    EXPECT_EQ(chunkRoots[0]->level_, 2);
    EXPECT_EQ(chunkRoots[1]->level_, 3);
}
