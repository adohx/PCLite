#include <gtest/gtest.h>
#include "node_loader/point_cloud_loader.h"
#include "converter.h"
#include "node.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "./test_data"
#endif

namespace {

// Converts the small, fully deterministic test_data/sample.las (see
// tools/generate_sample_las.py) into a fresh temp dataset for each test,
// the same way ConverterViewerFixture (test_converter_viewer_integration.cpp)
// already does with its own synthetic LAS -- so this only needs the one
// small committed .las fixture, never a frozen snapshot of converter
// output that could silently go stale if the PCLite format changes.
struct PointCloudLoaderTest : public ::testing::Test {
    std::filesystem::path tempDir;
    std::filesystem::path datasetDir;
    std::unique_ptr<PointCloudLoader> loader;

    void SetUp() override {
        // random_seed() alone (process-launch-time, second resolution) collides
        // when ctest -jN launches multiple test processes within the same
        // second, racing on the same temp dir; nanosecond clock makes that
        // collision practically impossible.
        tempDir = std::filesystem::temp_directory_path() /
                  ("pclite_pcl_loader_test_" +
                   std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::filesystem::remove_all(tempDir);
        std::filesystem::create_directories(tempDir);
        datasetDir = tempDir / "dataset";

        Converter::Options opts;
        opts.maxPointsPerChunk = 500;
        opts.firstChunkSize = 100;
        std::string lasPath = std::string(TEST_DATA_DIR) + "/sample.las";
        Converter converter({lasPath}, datasetDir.string(), opts);
        ASSERT_TRUE(converter.run()) << "Converter::run() failed";

        loader = std::make_unique<PointCloudLoader>(datasetDir.string());
    }

    void TearDown() override {
        // loader (and any Node it loaded) must release its open file handles
        // before we delete the directory they live in -- Windows refuses to
        // delete a file that's still open, unlike POSIX.
        loader.reset();
        std::filesystem::remove_all(tempDir);
    }
};

// Find first non-proxy leaf in a BFS from root (max depth 3 to keep it fast).
std::shared_ptr<Node> findLoadableNode(const std::shared_ptr<Node>& root, int maxDepth = 3) {
    if (!root) return nullptr;
    if (root->type_ != NodeType::Proxy && root->numPoints_ > 0)
        return root;
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

TEST_F(PointCloudLoaderTest, ConstructFromValidDirectory) {
    ASSERT_NO_THROW(PointCloudLoader other(datasetDir.string()));
}

TEST_F(PointCloudLoaderTest, ConstructFromInvalidDirectoryThrows) {
    EXPECT_THROW(PointCloudLoader("/nonexistent/path"), std::runtime_error);
}

// ── Metadata ──────────────────────────────────────────────────────────────────

TEST_F(PointCloudLoaderTest, BoundingBoxIsValid) {
    EXPECT_TRUE(loader->boundingBox().isValid());
}

TEST_F(PointCloudLoaderTest, BoundingBoxMinLessThanMax) {
    auto bb = loader->boundingBox();
    EXPECT_LT(bb.min().x, bb.max().x);
    EXPECT_LT(bb.min().y, bb.max().y);
    EXPECT_LT(bb.min().z, bb.max().z);
}

TEST_F(PointCloudLoaderTest, AttributesContainPosition) {
    // getAttribute is non-const; copy to allow the call
    auto attrs = loader->attributes();
    EXPECT_FALSE(attrs.getAttribute("position").name_.empty());
}

// ── loadRoot ──────────────────────────────────────────────────────────────────

TEST_F(PointCloudLoaderTest, LoadRootReturnsNonNull) {
    EXPECT_NE(loader->loadRoot(), nullptr);
}

TEST_F(PointCloudLoaderTest, LoadRootHasCorrectName) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->name_, "r");
}

TEST_F(PointCloudLoaderTest, LoadRootIsAtLevelZero) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->level_, 0);
}

TEST_F(PointCloudLoaderTest, LoadRootNotInLoadingState) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_FALSE(root->isLoading());
}

TEST_F(PointCloudLoaderTest, LoadRootBoundingBoxMatchesMetadata) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);
    auto rootBB = root->bb_;
    auto metaBB = loader->boundingBox();
    EXPECT_DOUBLE_EQ(rootBB.min().x, metaBB.min().x);
    EXPECT_DOUBLE_EQ(rootBB.min().y, metaBB.min().y);
    EXPECT_DOUBLE_EQ(rootBB.min().z, metaBB.min().z);
    EXPECT_DOUBLE_EQ(rootBB.max().x, metaBB.max().x);
    EXPECT_DOUBLE_EQ(rootBB.max().y, metaBB.max().y);
    EXPECT_DOUBLE_EQ(rootBB.max().z, metaBB.max().z);
}

// ── Tree structure ────────────────────────────────────────────────────────────

TEST_F(PointCloudLoaderTest, LoadRootProducesChildNodes) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);

    bool hasChild = false;
    for (const auto& c : root->children_)
        if (c) { hasChild = true; break; }

    EXPECT_TRUE(hasChild) << "root node should have at least one child after loading hierarchy";
}

// ── load() ────────────────────────────────────────────────────────────────────

TEST_F(PointCloudLoaderTest, LoadNonProxyNodeSetsLoadedFlag) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    bool ok = loader->load(target);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(target->isLoaded());
}

TEST_F(PointCloudLoaderTest, LoadedNodeHasPoints) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader->load(target);
    EXPECT_FALSE(target->getPoints().empty());
}

TEST_F(PointCloudLoaderTest, LoadAlreadyLoadedNodeIsIdempotent) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader->load(target);
    ASSERT_TRUE(target->isLoaded());

    // Second call should return immediately without side effects
    bool ok = loader->load(target);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(target->isLoaded());
}

TEST_F(PointCloudLoaderTest, PointCountMatchesNodeNumPoints) {
    auto root = loader->loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader->load(target);
    auto points = target->getPoints();
    EXPECT_EQ(points.size(), static_cast<size_t>(target->numPoints_));
}
