//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "attribute_handler/attribute_codec.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "concurrent_writer.h"
#include "hierarchy_builder.h"
#include "indexer.h"

namespace {

Attribute makePositionAttribute() {
    Attribute position;
    position.name_ = "position";
    position.numElements_ = 3;
    position.bytes_ = 12;
    position.type_ = AttributeType::INT32;
    position.scale_ = {0.001, 0.001, 0.001};
    position.offset_ = {0, 0, 0};
    return position;
}

void encodePosition(std::vector<uint8_t> &buf, size_t rowOffset, const vec3d &p, const Attribute &posAttr) {
    for (int i = 0; i < 3; ++i) {
        double comp = attribute_codec::component(p, i);
        double scale = attribute_codec::component(posAttr.scale_, i);
        attribute_codec::writeElement(buf.data() + rowOffset + i * 4, posAttr.type_, comp / scale);
    }
}

vec3d decodePosition(const uint8_t *row, const Attribute &posAttr) {
    double out[3] = {0, 0, 0};
    AttributeHandlerRegistry::get(posAttr)->decode(row, posAttr, out);
    return {out[0], out[1], out[2]};
}

double distance(const vec3d &a, const vec3d &b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void writeChunkFile(const std::filesystem::path &dir, const std::string &chunkName,
                     const std::vector<vec3d> &points, const Attribute &posAttr) {
    std::vector<uint8_t> buf(points.size() * 12);
    for (size_t i = 0; i < points.size(); ++i) {
        encodePosition(buf, i * 12, points[i], posAttr);
    }

    std::filesystem::path chunksDir = dir / "chunks";
    std::filesystem::create_directories(chunksDir);

    std::ofstream out(chunksDir / (chunkName + ".bin"), std::ios::binary);
    out.write(reinterpret_cast<const char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

std::vector<uint8_t> readFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Walks `root` with the same BFS order writeHierarchy() uses, returning the
// nodes in that order.
std::vector<std::shared_ptr<Node>> bfsOrder(const std::shared_ptr<Node> &root) {
    std::vector<std::shared_ptr<Node>> order;
    std::deque<std::shared_ptr<Node>> queue = {root};

    while (!queue.empty()) {
        std::shared_ptr<Node> node = queue.front();
        queue.pop_front();
        order.push_back(node);

        for (int c = 0; c < 8; ++c) {
            if (node->children_[c]) queue.push_back(node->children_[c]);
        }
    }

    return order;
}

// Validates the on-disk hierarchy.bin/octree.bin produced by mergeChunks
// against the in-memory tree rooted at `root`:
//  - hierarchy.bin holds exactly one 22-byte record per node, in BFS order,
//    matching that node's childMask_/type_/numPoints_/address_/byteSize_.
//  - every node's [address_, address_+byteSize_) lies within octree.bin.
//  - the total numPoints_ across all nodes equals expectedTotalPoints.
//  - for every non-root node, the points stored at that node are mutually at
//    least spacingOf(level) = rootSpacing / 2^level apart (the root is
//    exempt: its leftover "rejected" points are folded in unfiltered).
void verifyIndexedTree(const std::shared_ptr<Node> &root, const Attribute &posAttr, uint64_t rowStride,
                        const std::filesystem::path &dir, double rootSpacing, uint64_t expectedTotalPoints) {
    std::vector<std::shared_ptr<Node>> nodes = bfsOrder(root);

    std::vector<uint8_t> hierarchy = readFile(dir / "hierarchy.bin");
    ASSERT_EQ(hierarchy.size(), nodes.size() * 22u);

    uint64_t octreeSize = std::filesystem::file_size(dir / "octree.bin");
    std::vector<uint8_t> octree = readFile(dir / "octree.bin");

    uint64_t totalPoints = 0;

    for (size_t i = 0; i < nodes.size(); ++i) {
        const Node &node = *nodes[i];
        const uint8_t *rec = hierarchy.data() + i * 22;

        uint8_t type = rec[0];
        uint8_t childMask = rec[1];
        uint32_t numPoints;
        uint64_t address, byteSize;
        std::memcpy(&numPoints, rec + 2, 4);
        std::memcpy(&address, rec + 6, 8);
        std::memcpy(&byteSize, rec + 14, 8);

        EXPECT_EQ(type, static_cast<uint8_t>(node.type_)) << "node " << i << " (" << node.name_ << ")";
        EXPECT_EQ(childMask, node.childMask_) << "node " << i << " (" << node.name_ << ")";
        EXPECT_EQ(numPoints, node.numPoints_) << "node " << i << " (" << node.name_ << ")";
        EXPECT_EQ(address, node.address_) << "node " << i << " (" << node.name_ << ")";
        EXPECT_EQ(byteSize, node.byteSize_) << "node " << i << " (" << node.name_ << ")";

        EXPECT_EQ(byteSize, static_cast<uint64_t>(numPoints) * rowStride) << "node " << i << " (" << node.name_ << ")";
        EXPECT_LE(address + byteSize, octreeSize) << "node " << i << " (" << node.name_ << ")";

        totalPoints += numPoints;

        if (node.parent_ && numPoints > 0) {
            double spacing = rootSpacing / std::pow(2.0, node.level_);

            std::vector<vec3d> positions;
            for (uint32_t p = 0; p < numPoints; ++p) {
                positions.push_back(decodePosition(octree.data() + address + p * rowStride, posAttr));
            }

            for (size_t a = 0; a < positions.size(); ++a) {
                for (size_t b = a + 1; b < positions.size(); ++b) {
                    EXPECT_GE(distance(positions[a], positions[b]), spacing - 1e-9)
                        << "node " << node.name_ << ": points " << a << " and " << b << " too close";
                }
            }
        }
    }

    EXPECT_EQ(totalPoints, expectedTotalPoints);
}

} // namespace

class IndexerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("pclite_indexer_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    std::filesystem::path dir_;
};

// A single chunk "r" covers the whole root AABB. Its points form a 4x4x4
// lattice (spacing 2.0); with firstChunkSize=8 the local octree subdivides
// once (8 children, 8 points each). With rootSpacing=6.0, spacingOf(1)=3.0
// is large enough that some points within each child are rejected and bubble
// up to the root, which (per the root special case) folds its own rejected
// points back into accepted.
TEST_F(IndexerTest, SingleChunkCoversRootAndPropagatesRejectedToRoot) {
    Attribute posAttr = makePositionAttribute();
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    BoundingBoxd rootAABB({0, 0, 0}, {8, 8, 8});

    std::vector<vec3d> points;
    for (double x : {0.5, 2.5, 4.5, 6.5}) {
        for (double y : {0.5, 2.5, 4.5, 6.5}) {
            for (double z : {0.5, 2.5, 4.5, 6.5}) {
                points.push_back({x, y, z});
            }
        }
    }
    ASSERT_EQ(points.size(), 64u);
    writeChunkFile(dir_, "r", points, posAttr);

    std::vector<ChunkInfo> chunks;
    ChunkInfo chunk;
    chunk.name = "r";
    chunk.numPoints = points.size();
    chunks.push_back(chunk);

    std::vector<std::shared_ptr<Node>> chunkRoots;
    HierarchyBuilder builder(rootAABB);
    std::shared_ptr<Node> root = builder.build(chunks, chunkRoots);
    ASSERT_EQ(chunkRoots[0], root);

    auto writer = std::make_shared<ConcurrentWriter>(dir_.string());

    ConverterOptions options;
    options.firstChunkSize = 8;

    const double rootSpacing = 6.0;
    Indexer indexer(attrs, dir_.string(), writer, createSampler("poisson"), options, rootSpacing);

    ASSERT_TRUE(indexer.indexChunk(chunkRoots[0], chunks[0]));
    ASSERT_TRUE(indexer.mergeChunks(root, chunks));
    writer->flushAll();

    EXPECT_EQ(indexer.maxLevel(), 1);
    verifyIndexedTree(root, posAttr, 12, dir_, rootSpacing, points.size());
}

// Two chunks "r0" and "r7" occupy opposite octants of the root AABB. Each is
// subdivided into its own 8-leaf local octree (firstChunkSize=10, ~10
// points/leaf) and indexed independently; mergeChunks() then collects each
// chunk's leftover into the root.
TEST_F(IndexerTest, TwoChunksMergeIntoRoot) {
    Attribute posAttr = makePositionAttribute();
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    BoundingBoxd rootAABB({0, 0, 0}, {8, 8, 8});

    std::mt19937 rng(7);

    std::vector<vec3d> pointsR0;
    {
        std::uniform_real_distribution<double> dist(0.0, 4.0);
        for (int i = 0; i < 80; ++i) pointsR0.push_back({dist(rng), dist(rng), dist(rng)});
    }

    std::vector<vec3d> pointsR7;
    {
        std::uniform_real_distribution<double> dist(4.0, 8.0);
        for (int i = 0; i < 80; ++i) pointsR7.push_back({dist(rng), dist(rng), dist(rng)});
    }

    writeChunkFile(dir_, "r0", pointsR0, posAttr);
    writeChunkFile(dir_, "r7", pointsR7, posAttr);

    std::vector<ChunkInfo> chunks;
    {
        ChunkInfo c;
        c.name = "r0";
        c.numPoints = pointsR0.size();
        chunks.push_back(c);
    }
    {
        ChunkInfo c;
        c.name = "r7";
        c.numPoints = pointsR7.size();
        chunks.push_back(c);
    }

    std::vector<std::shared_ptr<Node>> chunkRoots;
    HierarchyBuilder builder(rootAABB);
    std::shared_ptr<Node> root = builder.build(chunks, chunkRoots);
    ASSERT_EQ(chunkRoots.size(), 2u);
    EXPECT_NE(chunkRoots[0], root);
    EXPECT_NE(chunkRoots[1], root);

    auto writer = std::make_shared<ConcurrentWriter>(dir_.string());

    ConverterOptions options;
    options.firstChunkSize = 10;

    const double rootSpacing = 2.0;
    Indexer indexer(attrs, dir_.string(), writer, createSampler("poisson"), options, rootSpacing);

    for (size_t i = 0; i < chunks.size(); ++i) {
        ASSERT_TRUE(indexer.indexChunk(chunkRoots[i], chunks[i]));
    }
    ASSERT_TRUE(indexer.mergeChunks(root, chunks));
    writer->flushAll();

    EXPECT_GE(indexer.maxLevel(), 2);
    verifyIndexedTree(root, posAttr, 12, dir_, rootSpacing, pointsR0.size() + pointsR7.size());
}
