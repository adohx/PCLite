#include <gtest/gtest.h>
#include "node_loader/point_cloud_loader.h"
#include "node.h"
#include <memory>
#include <string>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "./test_data"
#endif

static const std::string kDataDir = TEST_DATA_DIR;

// ── Construction ──────────────────────────────────────────────────────────────

TEST(PointCloudLoaderTest, ConstructFromValidDirectory) {
    ASSERT_NO_THROW(PointCloudLoader loader(kDataDir));
}

TEST(PointCloudLoaderTest, ConstructFromInvalidDirectoryThrows) {
    EXPECT_THROW(PointCloudLoader("/nonexistent/path"), std::runtime_error);
}

// ── Metadata ──────────────────────────────────────────────────────────────────

TEST(PointCloudLoaderTest, BoundingBoxIsValid) {
    PointCloudLoader loader(kDataDir);
    EXPECT_TRUE(loader.boundingBox().isValid());
}

TEST(PointCloudLoaderTest, BoundingBoxMinLessThanMax) {
    PointCloudLoader loader(kDataDir);
    auto bb = loader.boundingBox();
    EXPECT_LT(bb.min().x, bb.max().x);
    EXPECT_LT(bb.min().y, bb.max().y);
    EXPECT_LT(bb.min().z, bb.max().z);
}

TEST(PointCloudLoaderTest, AttributesContainPosition) {
    PointCloudLoader loader(kDataDir);
    // getAttribute is non-const; copy to allow the call
    auto attrs = loader.attributes();
    EXPECT_FALSE(attrs.getAttribute("position").name_.empty());
}

// ── loadRoot ──────────────────────────────────────────────────────────────────

TEST(PointCloudLoaderTest, LoadRootReturnsNonNull) {
    PointCloudLoader loader(kDataDir);
    EXPECT_NE(loader.loadRoot(), nullptr);
}

TEST(PointCloudLoaderTest, LoadRootHasCorrectName) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->name_, "r");
}

TEST(PointCloudLoaderTest, LoadRootIsAtLevelZero) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->level_, 0);
}

TEST(PointCloudLoaderTest, LoadRootNotInLoadingState) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_FALSE(root->isLoading());
}

TEST(PointCloudLoaderTest, LoadRootBoundingBoxMatchesMetadata) {
    PointCloudLoader loader(kDataDir);
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

// ── Tree structure ────────────────────────────────────────────────────────────

TEST(PointCloudLoaderTest, LoadRootProducesChildNodes) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    bool hasChild = false;
    for (const auto& c : root->children_)
        if (c) { hasChild = true; break; }

    EXPECT_TRUE(hasChild) << "root node should have at least one child after loading hierarchy";
}

// Find first non-proxy leaf in a BFS from root (max depth 3 to keep it fast).
static std::shared_ptr<Node> findLoadableNode(const std::shared_ptr<Node>& root, int maxDepth = 3) {
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

// ── load() ────────────────────────────────────────────────────────────────────

TEST(PointCloudLoaderTest, LoadNonProxyNodeSetsLoadedFlag) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    bool ok = loader.load(target);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(target->isLoaded());
}

TEST(PointCloudLoaderTest, LoadedNodeHasPoints) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader.load(target);
    EXPECT_FALSE(target->getPoints().empty());
}

TEST(PointCloudLoaderTest, LoadAlreadyLoadedNodeIsIdempotent) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader.load(target);
    ASSERT_TRUE(target->isLoaded());

    // Second call should return immediately without side effects
    bool ok = loader.load(target);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(target->isLoaded());
}

TEST(PointCloudLoaderTest, PointCountMatchesNodeNumPoints) {
    PointCloudLoader loader(kDataDir);
    auto root = loader.loadRoot();
    ASSERT_NE(root, nullptr);

    auto target = findLoadableNode(root);
    if (!target) GTEST_SKIP() << "no non-proxy node with points found in first 3 levels";

    loader.load(target);
    auto points = target->getPoints();
    EXPECT_EQ(points.size(), static_cast<size_t>(target->numPoints_));
}
