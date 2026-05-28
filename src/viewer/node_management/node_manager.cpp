#include "node_manager.h"
#include "camera/camera.h"
#include "manage_strategy.h"
// #include "node_loader/node_loader.h"
#include "painter/painter.h"
#include "../node_loader/node.h"

class NodeManager::NodeManagerPrivate {
public:
    std::vector<std::unique_ptr<Node>>           nodes_;
    std::vector<std::unique_ptr<ManageStrategy>> strategies_;
    std::vector<std::unique_ptr<Painter>>        painters_;
};

NodeManager::NodeManager() : d_(std::make_unique<NodeManagerPrivate>()) {}
NodeManager::~NodeManager() = default;

void NodeManager::addNode(std::unique_ptr<Node> node) {
    d_->nodes_.push_back(std::move(node));
}

void NodeManager::addStrategy(std::unique_ptr<ManageStrategy> strategy) {
    d_->strategies_.push_back(std::move(strategy));
}

void NodeManager::addPainter(std::unique_ptr<Painter> painter) {
    d_->painters_.push_back(std::move(painter));
}

void NodeManager::update(const Camera& camera) {
    for (auto& strategy : d_->strategies_) {
        auto* loader = strategy->nodeLoader();

        auto toLoad = strategy->computeNodesToLoad(camera, d_->nodes_);
        for (auto* node : toLoad) {
            if (!node->isLoaded() && loader) {
                // loader->bindCallback([node, &painters = d_->painters_](std::vector<uint8_t> data) {
                //     node->setData(std::move(data));
                //     for (auto& painter : painters)
                //         painter->addNode(node);
                // });
                // loader->setTarget(*node);
                // loader->load();
            }
        }

        auto toCull = strategy->computeNodesToCull(camera, d_->nodes_);
        for (auto* node : toCull) {
            if (node->isLoaded()) {
                for (auto& painter : d_->painters_)
                    painter->removeNode(node);
                node->clearData();
            }
        }
    }
}

void NodeManager::render(const Mat4f& viewMatrix) {
    for (auto& painter : d_->painters_)
        painter->paint(viewMatrix);
}
