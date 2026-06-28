#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "converter.h"
#include "node_loader/point_cloud_loader.h"
#include "node.h"
#include "vec3.h"

// ── Synthetic LAS writer ──────────────────────────────────────────────────────

namespace {

void writeRaw(std::ofstream& out, const void* data, size_t bytes) {
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(bytes));
}

template <typename T>
void writeValue(std::ofstream& out, T value) {
    writeRaw(out, &value, sizeof(T));
}

void writePadded(std::ofstream& out, const std::string& s, size_t fieldSize) {
    std::vector<char> buf(fieldSize, '\0');
    std::memcpy(buf.data(), s.data(), std::min(s.size(), fieldSize));
    writeRaw(out, buf.data(), fieldSize);
}

// LAS 1.2, point-data-format 2 (xyz + intensity + flags + classification +
// scan-angle + user-data + point-source-id + RGB = 26 bytes/point).
void writeSyntheticLas(const std::filesystem::path& path,
                       const std::vector<vec3d>& points,
                       const vec3d& scale,
                       const vec3d& offset) {
    std::vector<int32_t> ix(points.size()), iy(points.size()), iz(points.size());
    vec3d minW{std::numeric_limits<double>::infinity(),
               std::numeric_limits<double>::infinity(),
               std::numeric_limits<double>::infinity()};
    vec3d maxW{-std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity()};

    for (size_t i = 0; i < points.size(); ++i) {
        ix[i] = static_cast<int32_t>(std::llround((points[i].x - offset.x) / scale.x));
        iy[i] = static_cast<int32_t>(std::llround((points[i].y - offset.y) / scale.y));
        iz[i] = static_cast<int32_t>(std::llround((points[i].z - offset.z) / scale.z));

        vec3d world{ix[i] * scale.x + offset.x,
                    iy[i] * scale.y + offset.y,
                    iz[i] * scale.z + offset.z};
        minW.x = std::min(minW.x, world.x); minW.y = std::min(minW.y, world.y); minW.z = std::min(minW.z, world.z);
        maxW.x = std::max(maxW.x, world.x); maxW.y = std::max(maxW.y, world.y); maxW.z = std::max(maxW.z, world.z);
    }

    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());

    writeRaw(out, "LASF", 4);
    writeValue<uint16_t>(out, 0);  // file source id
    writeValue<uint16_t>(out, 0);  // global encoding
    writeValue<uint32_t>(out, 0);  // GUID data 1
    writeValue<uint16_t>(out, 0);
    writeValue<uint16_t>(out, 0);
    for (int i = 0; i < 8; ++i) writeValue<uint8_t>(out, 0);
    writeValue<uint8_t>(out, 1);   // version major
    writeValue<uint8_t>(out, 2);   // version minor
    writePadded(out, "PCLite", 32);
    writePadded(out, "PCLiteIntegTest", 32);
    writeValue<uint16_t>(out, 1);   // day of year
    writeValue<uint16_t>(out, 2026);
    writeValue<uint16_t>(out, 227); // header size
    writeValue<uint32_t>(out, 227); // offset to point data
    writeValue<uint32_t>(out, 0);   // num VLRs
    writeValue<uint8_t>(out, 2);    // point data format
    writeValue<uint16_t>(out, 26);  // record length
    writeValue<uint32_t>(out, static_cast<uint32_t>(points.size()));
    for (int i = 0; i < 5; ++i) writeValue<uint32_t>(out, 0); // points by return
    writeValue<double>(out, scale.x); writeValue<double>(out, scale.y); writeValue<double>(out, scale.z);
    writeValue<double>(out, offset.x); writeValue<double>(out, offset.y); writeValue<double>(out, offset.z);
    writeValue<double>(out, maxW.x); writeValue<double>(out, minW.x);
    writeValue<double>(out, maxW.y); writeValue<double>(out, minW.y);
    writeValue<double>(out, maxW.z); writeValue<double>(out, minW.z);

    for (size_t i = 0; i < points.size(); ++i) {
        writeValue<int32_t>(out, ix[i]);
        writeValue<int32_t>(out, iy[i]);
        writeValue<int32_t>(out, iz[i]);
        writeValue<uint16_t>(out, 0);                                      // intensity
        writeValue<uint8_t>(out, (1u << 3) | 1u);                         // 1 return of 1
        writeValue<uint8_t>(out, 2);                                       // classification
        writeValue<int8_t>(out, 0);                                        // scan angle
        writeValue<uint8_t>(out, 0);                                       // user data
        writeValue<uint16_t>(out, 0);                                      // point source id
        writeValue<uint16_t>(out, static_cast<uint16_t>(i % 256));        // R
        writeValue<uint16_t>(out, static_cast<uint16_t>((i * 3) % 256));  // G
        writeValue<uint16_t>(out, static_cast<uint16_t>((i * 7) % 256));  // B
    }
}

// ── Shared fixture ────────────────────────────────────────────────────────────

constexpr size_t kNumPoints = 2000;
// Coordinate range for synthetic points: [0, 100)^3
constexpr double kCoordMin = 0.0;
constexpr double kCoordMax = 100.0;

struct ConverterViewerFixture : public ::testing::Test {
    std::filesystem::path tempDir;
    std::filesystem::path octreeDir;

    void SetUp() override {
        // Nanosecond clock instead of random_seed() (process-launch-time,
        // second resolution) avoids temp dir collisions when ctest -jN
        // launches multiple test processes within the same second.
        tempDir = std::filesystem::temp_directory_path() /
                  ("pclite_integ_" +
                   std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::filesystem::remove_all(tempDir);
        std::filesystem::create_directories(tempDir);
        octreeDir = tempDir / "out";

        // Generate synthetic point cloud
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> dist(kCoordMin, kCoordMax);
        std::vector<vec3d> points;
        points.reserve(kNumPoints);
        for (size_t i = 0; i < kNumPoints; ++i)
            points.push_back({dist(rng), dist(rng), dist(rng)});

        auto lasPath = tempDir / "synthetic.las";
        writeSyntheticLas(lasPath, points, {0.001, 0.001, 0.001}, {0.0, 0.0, 0.0});

        Converter::Options opts;
        opts.maxPointsPerChunk = 500;
        opts.firstChunkSize = 100;
        Converter converter({lasPath.string()}, octreeDir.string(), opts);
        ASSERT_TRUE(converter.run()) << "Converter::run() failed";
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }
};

// Helper: BFS for first loadable (non-proxy, numPoints > 0) node within maxDepth.
std::shared_ptr<Node> findLoadableNode(const std::shared_ptr<Node>& root, int maxDepth = 4) {
    if (!root) return nullptr;
    if (root->type_ != NodeType::Proxy && root->numPoints_ > 0) return root;
    if (maxDepth == 0) return nullptr;
    for (const auto& child : root->children_) {
        if (!child) continue;
        auto found = findLoadableNode(child, maxDepth - 1);
        if (found) return found;
    }
    return nullptr;
}

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, LoaderConstructsFromConverterOutput) {
    ASSERT_NO_THROW(PointCloudLoader loader(octreeDir.string()));
}

// ── Bounding box ─────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, BoundingBoxIsValid) {
    PointCloudLoader loader(octreeDir.string());
    EXPECT_TRUE(loader.boundingBox().isValid());
}

TEST_F(ConverterViewerFixture, BoundingBoxIsWithinInputCoordinateRange) {
    PointCloudLoader loader(octreeDir.string());
    auto bb = loader.boundingBox();

    // The converter uses a cubic root AABB, so it may be slightly wider than
    // [0,100]^3; we only assert that the box is not wildly wrong.
    EXPECT_GE(bb.min().x, kCoordMin - 1.0);
    EXPECT_GE(bb.min().y, kCoordMin - 1.0);
    EXPECT_GE(bb.min().z, kCoordMin - 1.0);

    EXPECT_LE(bb.max().x, kCoordMax + 1.0);
    EXPECT_LE(bb.max().y, kCoordMax + 1.0);
    EXPECT_LE(bb.max().z, kCoordMax + 1.0);
}

// ── Attributes ───────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, AttributesContainPosition) {
    PointCloudLoader loader(octreeDir.string());
    auto attrs = loader.attributes();
    EXPECT_FALSE(attrs.getAttribute("position").name_.empty());
}

TEST_F(ConverterViewerFixture, AttributesContainRgb) {
    PointCloudLoader loader(octreeDir.string());
    auto attrs = loader.attributes();
    EXPECT_FALSE(attrs.getAttribute("rgb").name_.empty());
}

// ── Root node ─────────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, LoadRootReturnsNonNull) {
    PointCloudLoader loader(octreeDir.string());
    EXPECT_NE(loader.loadRoot(), nullptr);
}

TEST_F(ConverterViewerFixture, RootNameAndLevel) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->name_, "r");
    EXPECT_EQ(root->level_, 0);
}

TEST_F(ConverterViewerFixture, RootBoundingBoxMatchesMetadata) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto rootBB = root->bb_;
    auto metaBB = loader.boundingBox();
    EXPECT_DOUBLE_EQ(rootBB.min().x, metaBB.min().x);
    EXPECT_DOUBLE_EQ(rootBB.min().y, metaBB.min().y);
    EXPECT_DOUBLE_EQ(rootBB.min().z, metaBB.min().z);
    EXPECT_DOUBLE_EQ(rootBB.max().x, metaBB.max().x);
    EXPECT_DOUBLE_EQ(rootBB.max().y, metaBB.max().y);
    EXPECT_DOUBLE_EQ(rootBB.max().z, metaBB.max().z);
}

TEST_F(ConverterViewerFixture, RootHasAtLeastOneChild) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    bool hasChild = false;
    for (const auto& c : root->children_)
        if (c) { hasChild = true; break; }
    EXPECT_TRUE(hasChild);
}

// ── Point loading ─────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, LoadNodeSucceeds) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no loadable node found in first 4 levels";

    EXPECT_TRUE(loader.load(target));
    EXPECT_TRUE(target->isLoaded());
}

TEST_F(ConverterViewerFixture, LoadedNodePointCountMatchesMetadata) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no loadable node found in first 4 levels";

    loader.load(target);
    EXPECT_EQ(target->getPoints().size(), static_cast<size_t>(target->numPoints_));
}

TEST_F(ConverterViewerFixture, LoadedPointsAreWithinRootBoundingBox) {
    PointCloudLoader loader(octreeDir.string());
    auto bb = loader.boundingBox();
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no loadable node found in first 4 levels";

    loader.load(target);
    auto points = target->getPoints();
    ASSERT_FALSE(points.empty());

    // float precision may introduce small error vs. double bbox; use 0.01 slack
    constexpr float kEps = 0.01f;
    auto bmin = bb.min();
    auto bmax = bb.max();
    for (const auto& p : points) {
        EXPECT_GE(p.x, static_cast<float>(bmin.x) - kEps) << "point x below bbox min";
        EXPECT_GE(p.y, static_cast<float>(bmin.y) - kEps) << "point y below bbox min";
        EXPECT_GE(p.z, static_cast<float>(bmin.z) - kEps) << "point z below bbox min";
        EXPECT_LE(p.x, static_cast<float>(bmax.x) + kEps) << "point x above bbox max";
        EXPECT_LE(p.y, static_cast<float>(bmax.y) + kEps) << "point y above bbox max";
        EXPECT_LE(p.z, static_cast<float>(bmax.z) + kEps) << "point z above bbox max";
    }
}

// ── Idempotency ───────────────────────────────────────────────────────────────

TEST_F(ConverterViewerFixture, LoadNodeIsIdempotent) {
    PointCloudLoader loader(octreeDir.string());
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no loadable node found in first 4 levels";

    loader.load(target);
    ASSERT_TRUE(target->isLoaded());
    size_t firstCount = target->getPoints().size();

    EXPECT_TRUE(loader.load(target));
    EXPECT_EQ(target->getPoints().size(), firstCount);
}
