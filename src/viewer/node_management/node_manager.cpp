#include "node_manager.h"
#include "camera/camera.h"
#include "manage_strategy.h"
#include "painter/painter.h"
#include "../../core/node.h"
#include <unordered_set>
#include <vector>

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
    for (auto& painter : d_->painters_)
        painter->syncCamera(camera);

    for (auto& strategy : d_->strategies_) {
        auto* loader = strategy->nodeLoader();

        auto desired    = strategy->evaluate(camera, d_->nodes_);
        auto desiredSet = std::unordered_set<Node*>(desired.begin(), desired.end());

        // Load and paint newly desired nodes.
        for (Node* node : desired) {
            if (loader && !node->isLoaded() && !node->isLoading()) {
                auto sp = std::shared_ptr<Node>(node, [](Node*){});
                loader->load(sp);
            }
            if (node->isLoaded() && d_->inPainters_.insert(node).second) {
                for (auto& painter : d_->painters_)
                    painter->addNode(node);
            }
        }

        // Cull nodes that the strategy no longer wants.
        std::vector<Node*> toCull;
        for (Node* active : d_->inPainters_)
            if (!desiredSet.count(active))
                toCull.push_back(active);

        for (Node* node : toCull) {
            for (auto& painter : d_->painters_)
                painter->removeNode(node);
            d_->inPainters_.erase(node);
            node->clearData();
        }
    }
}

void NodeManager::render(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    for (auto& painter : d_->painters_)
        painter->paint(viewMatrix, projMatrix);
}

void NodeManager::renderPick(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    for (auto& painter : d_->painters_)
        painter->paintPick(viewMatrix, projMatrix);
}

Node* NodeManager::nodeForId(uint32_t id) const {
    for (auto& painter : d_->painters_)
        if (Node* node = painter->nodeForId(id))
            return node;
    return nullptr;
}

void NodeManager::setHighlight(Node* node, int pointIndex) {
    for (auto& painter : d_->painters_)
        painter->setHighlight(node, pointIndex);
}

void NodeManager::setPlaneFit(bool active, const vec3f& center, const vec3f& normal, float radius) {
    for (auto& painter : d_->painters_)
        painter->setPlaneFit(active, center, normal, radius);
}

std::vector<Node*> NodeManager::nodesNear(const vec3d& point, double radius) const {
    std::vector<Node*> result;
    for (Node* node : d_->inPainters_) {
        vec3d d = node->tightBB_ - point;
        if (d.x * d.x + d.y * d.y + d.z * d.z <= radius * radius)
            result.push_back(node);
    }
    return result;
}
