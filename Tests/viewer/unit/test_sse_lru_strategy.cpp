#include <gtest/gtest.h>
#include "node_management/sse_lru_strategy.h"
#include "camera/perspective_camera.h"
#include "node.h"
#include "bounding_box.h"

namespace {
std::shared_ptr<Node> makeLoadedLeaf(const std::string& name, double offsetX) {
    auto n = std::make_shared<Node>(name, BoundingBoxd({offsetX, 0, 0}, {offsetX + 1, 1, 1}));
    n->type_ = NodeType::Leaf;
    n->setLoaded(true);
    n->spacing_ = 0.1;
    return n;
}
}

// SSELruStrategy must never evict a node its own traversal just determined
// is needed this frame, even when that exceeds maxLoadedNodes_ -- doing so
// would just make it reload from disk next frame only to be evicted again
// (permanent thrash: displayed point count oscillating, FPS collapsing
// under constant disk I/O for a working set that's genuinely larger than
// the budget at the current camera position).
TEST(SSELruStrategyTest, DoesNotEvictNodesNeededThisFrameEvenOverBudget) {
    SSELruStrategy strategy(/*screenHeight=*/1080, /*sseThreshold=*/1.0f, /*maxLoadedNodes=*/2);
    PerspectiveCamera cam;
    cam.setPosition({0, 0, 100});
    cam.setTarget({0, 0, 0});

    std::vector<std::shared_ptr<Node>> nodes;
    for (int i = 0; i < 4; ++i) nodes.push_back(makeLoadedLeaf("n" + std::to_string(i), i * 2.0));

    auto active = strategy.evaluate(cam, nodes);

    EXPECT_EQ(active.size(), 4u);
}

// Once a node is no longer touched (not needed by the current frame),
// ordinary LRU eviction down to the budget still applies.
TEST(SSELruStrategyTest, EvictsNodesNoLongerNeededDownToBudget) {
    SSELruStrategy strategy(/*screenHeight=*/1080, /*sseThreshold=*/1.0f, /*maxLoadedNodes=*/1);
    PerspectiveCamera cam;
    cam.setPosition({0, 0, 100});
    cam.setTarget({0, 0, 0});

    auto a = makeLoadedLeaf("a", 0.0);
    auto b = makeLoadedLeaf("b", 2.0);
    strategy.evaluate(cam, {a, b}); // both touched; over budget but neither evicted (see above)

    // Next frame: nothing is touched (empty tree set stands in for "neither
    // node is needed anymore") -- now eviction should trim down to budget.
    auto second = strategy.evaluate(cam, {});
    EXPECT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0], b.get()); // b was touched more recently than a
}
