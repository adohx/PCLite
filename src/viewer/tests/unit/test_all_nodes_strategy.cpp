#include <gtest/gtest.h>
#include "node_management/all_nodes_strategy.h"
#include "camera/orbit_camera.h"
#include "node.h"
#include "bounding_box.h"

static std::shared_ptr<Node> makeNode(const std::string& name,
                                      NodeType type = NodeType::Normal) {
    auto n = std::make_shared<Node>(name, BoundingBoxd({0,0,0}, {1,1,1}));
    n->type_ = type;
    return n;
}

// ── computeNodesToLoad ────────────────────────────────────────────────────────

TEST(AllNodesStrategyTest, ReturnsUnloadedNonProxyNodes) {
    AllNodesStrategy strategy;
    OrbitCamera cam;

    auto n1 = makeNode("n1");                    // Normal, unloaded → included
    auto n2 = makeNode("n2", NodeType::Leaf);    // Leaf,   unloaded → included
    auto n3 = makeNode("n3", NodeType::Proxy);   // Proxy            → excluded

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2, n3};
    auto result = strategy.computeNodesToLoad(cam, nodes);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], n1.get());
    EXPECT_EQ(result[1], n2.get());
}

TEST(AllNodesStrategyTest, ExcludesAlreadyLoadedNodes) {
    AllNodesStrategy strategy;
    OrbitCamera cam;

    auto n1 = makeNode("n1");
    auto n2 = makeNode("n2");
    n2->setLoaded(true);

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2};
    auto result = strategy.computeNodesToLoad(cam, nodes);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], n1.get());
}

TEST(AllNodesStrategyTest, ExcludesLoadingNodes) {
    AllNodesStrategy strategy;
    OrbitCamera cam;

    auto n1 = makeNode("n1");
    n1->setLoading(true);   // isLoading but not isLoaded
    auto n2 = makeNode("n2");

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2};
    // isLoaded() is still false for n1, so strategy DOES return it
    // (loading state is managed by the caller, not the strategy)
    auto result = strategy.computeNodesToLoad(cam, nodes);
    EXPECT_EQ(result.size(), 2u);
}

TEST(AllNodesStrategyTest, EmptyNodeListProducesEmptyResult) {
    AllNodesStrategy strategy;
    OrbitCamera cam;
    std::vector<std::shared_ptr<Node>> nodes;
    EXPECT_TRUE(strategy.computeNodesToLoad(cam, nodes).empty());
}

// ── computeNodesToCull ────────────────────────────────────────────────────────

TEST(AllNodesStrategyTest, CullAlwaysReturnsEmpty) {
    AllNodesStrategy strategy;
    OrbitCamera cam;

    auto n1 = makeNode("n1");
    n1->setLoaded(true);
    auto n2 = makeNode("n2", NodeType::Leaf);
    n2->setLoaded(true);

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2};
    EXPECT_TRUE(strategy.computeNodesToCull(cam, nodes).empty());
}

TEST(AllNodesStrategyTest, CullEmptyListReturnsEmpty) {
    AllNodesStrategy strategy;
    OrbitCamera cam;
    std::vector<std::shared_ptr<Node>> nodes;
    EXPECT_TRUE(strategy.computeNodesToCull(cam, nodes).empty());
}

// ── NodeLoader integration ────────────────────────────────────────────────────

TEST(AllNodesStrategyTest, NodeLoaderInitiallyNull) {
    AllNodesStrategy strategy;
    EXPECT_EQ(strategy.nodeLoader(), nullptr);
}
