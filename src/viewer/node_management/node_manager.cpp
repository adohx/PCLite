#include "node_manager.h"
#include "camera/camera.h"
#include "manage_strategy.h"
#include "painter/painter.h"
#include "../../core/node.h"
#include <unordered_set>

class NodeManager::NodeManagerPrivate {
public:
    std::vector<std::shared_ptr<Node>>           nodes_;
    std::unordered_set<Node*>                    inPainters_;
    std::vector<std::unique_ptr<ManageStrategy>> strategies_;
    std::vector<std::unique_ptr<Painter>>        painters_;
};

NodeManager::NodeManager() : d_(std::make_unique<NodeManagerPrivate>()) {}
NodeManager::~NodeManager() = default;

void NodeManager::addNode(std::shared_ptr<Node> node) {
    d_->nodes_.push_back(std::move(node));
}

void NodeManager::addTree(std::shared_ptr<Node> root) {
    if (!root) return;
    if (root->type_ != NodeType::Proxy)
        d_->nodes_.push_back(root);
    for (auto& child : root->children_)
        if (child) addTree(child);
}

void NodeManager::addStrategy(std::unique_ptr<ManageStrategy> strategy) {
    d_->strategies_.push_back(std::move(strategy));
}

void NodeManager::addPainter(std::unique_ptr<Painter> painter) {
    d_->painters_.push_back(std::move(painter));
}

void NodeManager::update(const Camera& camera) {
    // First pass: nodes already loaded (e.g. synchronously by loadRoot) but not yet in painters.
    for (auto& n : d_->nodes_) {
        if (n->isLoaded() && d_->inPainters_.insert(n.get()).second) {
            for (auto& painter : d_->painters_)
                painter->addNode(n.get());
        }
    }

    for (auto& strategy : d_->strategies_) {
        auto* loader = strategy->nodeLoader();

        // Second pass: nodes that still need loading.
        auto toLoad = strategy->computeNodesToLoad(camera, d_->nodes_);
        for (auto* node : toLoad) {
            if (loader) {
                auto sp = std::shared_ptr<Node>(node, [](Node*){});
                loader->load(sp);
                if (node->isLoaded() && d_->inPainters_.insert(node).second) {
                    for (auto& painter : d_->painters_)
                        painter->addNode(node);
                }
            }
        }

        auto toCull = strategy->computeNodesToCull(camera, d_->nodes_);
        for (auto* node : toCull) {
            if (node->isLoaded()) {
                for (auto& painter : d_->painters_)
                    painter->removeNode(node);
                d_->inPainters_.erase(node);
                node->clearData();
            }
        }
    }
}

void NodeManager::render(const Mat4f& viewMatrix) {
    for (auto& painter : d_->painters_)
        painter->paint(viewMatrix);
}
