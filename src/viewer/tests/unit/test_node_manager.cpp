#include <gtest/gtest.h>
#include "node_management/node_manager.h"
#include "node_management/manage_strategy.h"
#include "node_loader/node_loader.h"
#include "painter/painter.h"
#include "camera/orbit_camera.h"
#include "node.h"
#include "bounding_box.h"
#include "mat.h"
#include <memory>
#include <vector>

// ── Mocks ─────────────────────────────────────────────────────────────────────

class MockPainter : public Painter {
public:
    void addNode(Node* node) override    { added_.push_back(node); }
    void removeNode(Node* node) override { removed_.push_back(node); }
    void paint(const Mat4f&) override    { ++paintCalls_; }

    std::vector<Node*> added_;
    std::vector<Node*> removed_;
    int paintCalls_ = 0;
};

class MockLoader : public NodeLoader<Node> {
public:
    bool load(std::shared_ptr<Node> node) override {
        node->setLoaded(true);
        loadedNodes_.push_back(node.get());
        return true;
    }
    std::shared_ptr<Node> loadRoot() override { return nullptr; }

    std::vector<Node*> loadedNodes_;
};

class MockStrategy : public ManageStrategy {
public:
    std::vector<Node*> toLoad_;
    std::vector<Node*> toCull_;

    std::vector<Node*> computeNodesToLoad(
        const Camera&,
        const std::vector<std::shared_ptr<Node>>&) override { return toLoad_; }

    std::vector<Node*> computeNodesToCull(
        const Camera&,
        const std::vector<std::shared_ptr<Node>>&) override { return toCull_; }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::shared_ptr<Node> makeNode(const std::string& name,
                                      NodeType type = NodeType::Normal) {
    auto n = std::make_shared<Node>(name, BoundingBoxd({0,0,0}, {1,1,1}));
    n->type_ = type;
    return n;
}

// ── addNode / addTree ─────────────────────────────────────────────────────────

TEST(NodeManagerTest, AddTreeSkipsProxyRoot) {
    NodeManager mgr;
    auto* painter = new MockPainter;
    auto* strategy = new MockStrategy;
    MockLoader loader;
    strategy->setNodeLoader(&loader);

    auto root  = makeNode("r", NodeType::Proxy);
    auto child = makeNode("r0");
    root->children_[0] = child;
    strategy->toLoad_ = {child.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addTree(root);

    OrbitCamera cam;
    mgr.update(cam);

    // Proxy root must never reach the painter; only the child does.
    ASSERT_EQ(loader.loadedNodes_.size(), 1u);
    EXPECT_EQ(loader.loadedNodes_[0], child.get());
    ASSERT_EQ(painter->added_.size(), 1u);
    EXPECT_EQ(painter->added_[0], child.get());
}

TEST(NodeManagerTest, AddTreeIncludesAllNonProxyDescendants) {
    NodeManager mgr;
    auto* painter = new MockPainter;
    auto* strategy = new MockStrategy;
    MockLoader loader;
    strategy->setNodeLoader(&loader);

    auto root   = makeNode("r", NodeType::Proxy);
    auto child0 = makeNode("r0");
    auto child1 = makeNode("r1");
    root->children_[0] = child0;
    root->children_[1] = child1;
    strategy->toLoad_ = {child0.get(), child1.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addTree(root);

    OrbitCamera cam;
    mgr.update(cam);

    EXPECT_EQ(loader.loadedNodes_.size(), 2u);
    EXPECT_EQ(painter->added_.size(), 2u);
}

// ── update: already-loaded nodes ──────────────────────────────────────────────

TEST(NodeManagerTest, UpdateAddsAlreadyLoadedNodesToPainters) {
    NodeManager mgr;
    auto node = makeNode("n");
    node->setLoaded(true);

    auto* painter = new MockPainter;
    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);

    ASSERT_EQ(painter->added_.size(), 1u);
    EXPECT_EQ(painter->added_[0], node.get());
}

TEST(NodeManagerTest, UpdateDoesNotAddSameNodeTwice) {
    NodeManager mgr;
    auto node = makeNode("n");
    node->setLoaded(true);

    auto* painter = new MockPainter;
    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);
    mgr.update(cam);

    EXPECT_EQ(painter->added_.size(), 1u);
}

// ── update: strategy-driven loading ──────────────────────────────────────────

TEST(NodeManagerTest, UpdateLoadsUnloadedNodesViaStrategy) {
    NodeManager mgr;
    auto node = makeNode("n");

    auto* painter  = new MockPainter;
    auto* strategy = new MockStrategy;
    MockLoader loader;
    strategy->setNodeLoader(&loader);
    strategy->toLoad_ = {node.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);

    EXPECT_TRUE(node->isLoaded());
    ASSERT_EQ(painter->added_.size(), 1u);
    EXPECT_EQ(painter->added_[0], node.get());
}

TEST(NodeManagerTest, UpdateWithoutLoaderDoesNotLoad) {
    NodeManager mgr;
    auto node = makeNode("n");

    auto* painter  = new MockPainter;
    auto* strategy = new MockStrategy;
    // No loader set → strategy->nodeLoader() returns nullptr
    strategy->toLoad_ = {node.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);

    EXPECT_FALSE(node->isLoaded());
    EXPECT_TRUE(painter->added_.empty());
}

// ── update: culling ───────────────────────────────────────────────────────────

TEST(NodeManagerTest, UpdateCullsLoadedNodes) {
    NodeManager mgr;
    auto node = makeNode("n");
    node->setLoaded(true);

    auto* painter  = new MockPainter;
    auto* strategy = new MockStrategy;
    strategy->toCull_ = {node.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);

    // First pass adds to painter; cull pass removes and clears data
    EXPECT_FALSE(node->isLoaded());
    ASSERT_EQ(painter->removed_.size(), 1u);
    EXPECT_EQ(painter->removed_[0], node.get());
}

TEST(NodeManagerTest, CulledNodeCanBeReAddedAfterReload) {
    NodeManager mgr;
    auto node = makeNode("n");
    node->setLoaded(true);

    auto* painter  = new MockPainter;
    auto* strategy = new MockStrategy;
    MockLoader loader;
    strategy->setNodeLoader(&loader);
    strategy->toCull_ = {node.get()};

    mgr.addPainter(std::unique_ptr<Painter>(painter));
    mgr.addStrategy(std::unique_ptr<ManageStrategy>(strategy));
    mgr.addNode(node);

    OrbitCamera cam;
    mgr.update(cam);   // first pass: add → cull
    EXPECT_FALSE(node->isLoaded());
    EXPECT_EQ(painter->added_.size(), 1u);
    EXPECT_EQ(painter->removed_.size(), 1u);

    // Now make strategy load it again instead of culling
    strategy->toCull_.clear();
    strategy->toLoad_ = {node.get()};
    mgr.update(cam);

    EXPECT_TRUE(node->isLoaded());
    EXPECT_EQ(painter->added_.size(), 2u);  // added again after reload
}

// ── render ────────────────────────────────────────────────────────────────────

TEST(NodeManagerTest, RenderCallsPaintOnAllPainters) {
    NodeManager mgr;

    auto* p1 = new MockPainter;
    auto* p2 = new MockPainter;
    mgr.addPainter(std::unique_ptr<Painter>(p1));
    mgr.addPainter(std::unique_ptr<Painter>(p2));

    mgr.render(Mat4f::identity());

    EXPECT_EQ(p1->paintCalls_, 1);
    EXPECT_EQ(p2->paintCalls_, 1);
}

TEST(NodeManagerTest, RenderPassesViewMatrixToPainter) {
    NodeManager mgr;

    class CapturePainter : public Painter {
    public:
        void addNode(Node*) override {}
        void removeNode(Node*) override {}
        void paint(const Mat4f& m) override { lastMatrix_ = m; }
        Mat4f lastMatrix_;
    };

    auto* cap = new CapturePainter;
    mgr.addPainter(std::unique_ptr<Painter>(cap));

    auto id = Mat4f::identity();
    mgr.render(id);

    EXPECT_EQ(cap->lastMatrix_, id);
}

TEST(NodeManagerTest, RenderOnEmptyManagerDoesNotCrash) {
    NodeManager mgr;
    EXPECT_NO_THROW(mgr.render(Mat4f::identity()));
}
