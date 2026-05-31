#ifndef PCLITE_MANAGE_STRATEGY_H
#define PCLITE_MANAGE_STRATEGY_H

#include <memory>
#include <vector>
#include "../../core/node.h"
#include "node_loader/node_loader.h"

class Camera;

class ManageStrategy {
public:
    virtual ~ManageStrategy() = default;

    virtual std::vector<Node*> computeNodesToLoad(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) = 0;

    virtual std::vector<Node*> computeNodesToCull(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) = 0;

    void setNodeLoader(NodeLoader<Node>* loader);
    NodeLoader<Node>* nodeLoader() const;

protected:
    NodeLoader<Node>* loader_ = nullptr;
};

#endif //PCLITE_MANAGE_STRATEGY_H
