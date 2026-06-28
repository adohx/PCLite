#include <gtest/gtest.h>
#include "node_management/all_nodes_strategy.h"
#include "camera/perspective_camera.h"
#include "node.h"
#include "bounding_box.h"

static std::shared_ptr<Node> makeNode(const std::string& name,
                                      NodeType type = NodeType::Normal) {
    auto n = std::make_shared<Node>(name, BoundingBoxd({0,0,0}, {1,1,1}));
    n->type_ = type;
    return n;
}

// ── evaluate ──────────────────────────────────────────────────────────────────
// AllNodesStrategy::evaluate returns every non-proxy node regardless of load state.

TEST(AllNodesStrategyTest, ReturnsAllNonProxyNodes) {
    AllNodesStrategy strategy;
    PerspectiveCamera cam;

    auto n1 = makeNode("n1");                    // Normal, unloaded → included
    auto n2 = makeNode("n2", NodeType::Leaf);    // Leaf,   unloaded → included
    auto n3 = makeNode("n3", NodeType::Proxy);   // Proxy            → excluded

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2, n3};
    auto result = strategy.evaluate(cam, nodes);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], n1.get());
    EXPECT_EQ(result[1], n2.get());
}

TEST(AllNodesStrategyTest, IncludesAlreadyLoadedNodes) {
    AllNodesStrategy strategy;
    PerspectiveCamera cam;

    auto n1 = makeNode("n1");
    auto n2 = makeNode("n2");
    n2->setLoaded(true);

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2};
    auto result = strategy.evaluate(cam, nodes);

    ASSERT_EQ(result.size(), 2u);
}

TEST(AllNodesStrategyTest, IncludesLoadingNodes) {
    AllNodesStrategy strategy;
    PerspectiveCamera cam;

    auto n1 = makeNode("n1");
    n1->setLoading(true);
    auto n2 = makeNode("n2");

    std::vector<std::shared_ptr<Node>> nodes = {n1, n2};
    auto result = strategy.evaluate(cam, nodes);
    EXPECT_EQ(result.size(), 2u);
}

TEST(AllNodesStrategyTest, EmptyNodeListProducesEmptyResult) {
    AllNodesStrategy strategy;
    PerspectiveCamera cam;
    std::vector<std::shared_ptr<Node>> nodes;
    EXPECT_TRUE(strategy.evaluate(cam, nodes).empty());
}

TEST(AllNodesStrategyTest, ExcludesOnlyProxyNodes) {
    AllNodesStrategy strategy;
    PerspectiveCamera cam;

    auto proxy = makeNode("p", NodeType::Proxy);
    std::vector<std::shared_ptr<Node>> nodes = {proxy};
    EXPECT_TRUE(strategy.evaluate(cam, nodes).empty());
}

// ── NodeLoader integration ────────────────────────────────────────────────────

TEST(AllNodesStrategyTest, NodeLoaderInitiallyNull) {
    AllNodesStrategy strategy;
    EXPECT_EQ(strategy.nodeLoader(), nullptr);
}
