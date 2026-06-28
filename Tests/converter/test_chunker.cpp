//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "chunker.h"
#include "concurrent_writer.h"
#include "mock_attribute_reader.h"

namespace {

Attributes makeAttributes() {
    Attributes attrs;

    Attribute position;
    position.name_ = "position";
    position.numElements_ = 3;
    position.bytes_ = 12;
    position.type_ = AttributeType::INT32;
    position.scale_ = {0.001, 0.001, 0.001};
    position.offset_ = {0, 0, 0};
    attrs.pushAttribute(position);

    Attribute intensity;
    intensity.name_ = "intensity";
    intensity.numElements_ = 1;
    intensity.bytes_ = 2;
    intensity.type_ = AttributeType::UINT16;
    intensity.scale_ = {1, 1, 1};
    intensity.offset_ = {0, 0, 0};
    attrs.pushAttribute(intensity);

    return attrs;
}

std::vector<uint8_t> readFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

class ChunkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("pclite_chunker_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    std::filesystem::path dir_;
};

// A single tight cluster of points falls into one finest-grid cell. With
// maxPointsPerChunk large enough that the merge reaches level 0, that cell
// merges all the way up to the root, producing a single chunk "r" covering
// the whole grid.
TEST_F(ChunkerTest, SingleClusterMergesUpToRootChunk) {
    std::vector<vec3d> positions = {
        {0, 0, 0}, {0.01, 0.01, 0.01}, {0.02, 0.02, 0.02}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {8, 8, 8},
    };

    auto reader = std::make_shared<MockAttributeReader>(makeAttributes(), positions);
    auto writer = std::make_shared<ConcurrentWriter>(dir_.string());

    Chunker chunker({reader}, makeAttributes(), dir_.string(), writer, ConverterOptions{.maxPointsPerChunk = 1000});
    ASSERT_TRUE(chunker.run());
    writer->flushAll();

    ASSERT_EQ(chunker.chunks().size(), 1u);
    EXPECT_EQ(chunker.chunks()[0].name, "r");
    EXPECT_EQ(chunker.chunks()[0].level, 0);
    EXPECT_EQ(chunker.chunks()[0].size, 128u);
    EXPECT_EQ(chunker.chunks()[0].numPoints, positions.size());
}

// Two clusters at opposite corners of the cube, each larger than
// maxPointsPerChunk, finalize independently at the finest level (128) and
// never merge with each other, producing two chunks named after their
// octree path ("r0000000" / "r7777777").
TEST_F(ChunkerTest, TwoDistantClustersProduceTwoChunks) {
    std::vector<vec3d> clusterA = {{0, 0, 0}, {0.01, 0.01, 0.01}, {0.02, 0.02, 0.02}};
    std::vector<vec3d> clusterB = {{7.95, 7.95, 7.95}, {7.96, 7.96, 7.96}, {7.97, 7.97, 7.97}, {8, 8, 8}};

    std::vector<vec3d> positions;
    positions.insert(positions.end(), clusterA.begin(), clusterA.end());
    positions.insert(positions.end(), clusterB.begin(), clusterB.end());

    auto reader = std::make_shared<MockAttributeReader>(makeAttributes(), positions);
    auto writer = std::make_shared<ConcurrentWriter>(dir_.string());

    Chunker chunker({reader}, makeAttributes(), dir_.string(), writer, ConverterOptions{.maxPointsPerChunk = 2});
    ASSERT_TRUE(chunker.run());
    writer->flushAll();

    const BoundingBoxd &aabb = chunker.aabb();
    EXPECT_DOUBLE_EQ(aabb.min().x, 0.0);
    EXPECT_DOUBLE_EQ(aabb.min().y, 0.0);
    EXPECT_DOUBLE_EQ(aabb.min().z, 0.0);
    EXPECT_DOUBLE_EQ(aabb.max().x, 8.0);
    EXPECT_DOUBLE_EQ(aabb.max().y, 8.0);
    EXPECT_DOUBLE_EQ(aabb.max().z, 8.0);

    ASSERT_EQ(chunker.chunks().size(), 2u);

    const ChunkInfo &chunkA = chunker.chunks()[0];
    EXPECT_EQ(chunkA.name, "r0000000");
    EXPECT_EQ(chunkA.level, 7);
    EXPECT_EQ(chunkA.size, 1u);
    EXPECT_EQ(chunkA.numPoints, clusterA.size());
    EXPECT_DOUBLE_EQ(chunkA.bb.min().x, 0.0);
    EXPECT_DOUBLE_EQ(chunkA.bb.max().x, 0.0625);

    const ChunkInfo &chunkB = chunker.chunks()[1];
    EXPECT_EQ(chunkB.name, "r7777777");
    EXPECT_EQ(chunkB.level, 7);
    EXPECT_EQ(chunkB.size, 1u);
    EXPECT_EQ(chunkB.numPoints, clusterB.size());
    EXPECT_DOUBLE_EQ(chunkB.bb.min().x, 7.9375);
    EXPECT_DOUBLE_EQ(chunkB.bb.max().x, 8.0);

    // Per-chunk files were written with one 14-byte row (12B position +
    // 2B intensity) per assigned point.
    auto fileA = readFile(dir_ / "chunks" / (chunkA.name + ".bin"));
    auto fileB = readFile(dir_ / "chunks" / (chunkB.name + ".bin"));
    EXPECT_EQ(fileA.size(), clusterA.size() * 14u);
    EXPECT_EQ(fileB.size(), clusterB.size() * 14u);

    // Global position stats span both clusters.
    for (const Attribute &attr : chunker.attributes()) {
        if (attr.name_ == "position") {
            EXPECT_DOUBLE_EQ(attr.min_.x, 0.0);
            EXPECT_DOUBLE_EQ(attr.min_.y, 0.0);
            EXPECT_DOUBLE_EQ(attr.min_.z, 0.0);
            EXPECT_DOUBLE_EQ(attr.max_.x, 8.0);
            EXPECT_DOUBLE_EQ(attr.max_.y, 8.0);
            EXPECT_DOUBLE_EQ(attr.max_.z, 8.0);
            // offset_ is rebased to aabb_.min().
            EXPECT_DOUBLE_EQ(attr.offset_.x, 0.0);
            EXPECT_DOUBLE_EQ(attr.offset_.y, 0.0);
            EXPECT_DOUBLE_EQ(attr.offset_.z, 0.0);
        }
    }
}

TEST_F(ChunkerTest, WriteMetadataProducesExpectedJson) {
    std::vector<vec3d> clusterA = {{0, 0, 0}, {0.01, 0.01, 0.01}, {0.02, 0.02, 0.02}};
    std::vector<vec3d> clusterB = {{7.95, 7.95, 7.95}, {7.96, 7.96, 7.96}, {7.97, 7.97, 7.97}, {8, 8, 8}};

    std::vector<vec3d> positions;
    positions.insert(positions.end(), clusterA.begin(), clusterA.end());
    positions.insert(positions.end(), clusterB.begin(), clusterB.end());

    auto reader = std::make_shared<MockAttributeReader>(makeAttributes(), positions);
    auto writer = std::make_shared<ConcurrentWriter>(dir_.string());

    ConverterOptions options;
    options.maxPointsPerChunk = 2;
    options.firstChunkSize = 1234;
    options.stepSize = 4;

    Chunker chunker({reader}, makeAttributes(), dir_.string(), writer, options);
    ASSERT_TRUE(chunker.run());
    writer->flushAll();

    std::ifstream in(dir_ / "metadata.json");
    ASSERT_TRUE(in.is_open());
    nlohmann::json j;
    in >> j;

    EXPECT_EQ(j["version"], "2.0");
    EXPECT_EQ(j["points"].get<uint64_t>(), positions.size());
    EXPECT_EQ(j["encoding"], "DEFAULT");

    EXPECT_DOUBLE_EQ(j["offset"][0].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["offset"][1].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["offset"][2].get<double>(), 0.0);

    EXPECT_DOUBLE_EQ(j["scale"][0].get<double>(), 0.001);
    EXPECT_DOUBLE_EQ(j["spacing"].get<double>(), 8.0 / 128.0);

    EXPECT_DOUBLE_EQ(j["boundingBox"]["min"][0].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["boundingBox"]["max"][0].get<double>(), 8.0);

    EXPECT_EQ(j["hierarchy"]["firstChunkSize"].get<uint32_t>(), 1234u);
    EXPECT_EQ(j["hierarchy"]["stepSize"].get<uint32_t>(), 4u);
    EXPECT_EQ(j["hierarchy"]["depth"].get<int>(), 0);

    ASSERT_EQ(j["attributes"].size(), 2u);

    const nlohmann::json &posAttr = j["attributes"][0];
    EXPECT_EQ(posAttr["name"], "position");
    EXPECT_EQ(posAttr["type"], "int32");
    EXPECT_EQ(posAttr["numElements"].get<int>(), 3);
    EXPECT_DOUBLE_EQ(posAttr["min"][0].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(posAttr["max"][0].get<double>(), 8.0);

    const nlohmann::json &intensityAttr = j["attributes"][1];
    EXPECT_EQ(intensityAttr["name"], "intensity");
    EXPECT_EQ(intensityAttr["type"], "uint16");
    EXPECT_DOUBLE_EQ(intensityAttr["min"][0].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(intensityAttr["max"][0].get<double>(), 0.0);
}
