#ifndef PCLITE_MANAGE_STRATEGY_H
#define PCLITE_MANAGE_STRATEGY_H

#include <memory>
#include <vector>
#include "node.h"

class Camera;
class NodeLoader;

class ManageStrategy {
public:
    virtual ~ManageStrategy() = default;

    virtual std::vector<Node*> computeNodesToLoad(
        const Camera& camera,
        const std::vector<std::unique_ptr<Node>>& nodes) = 0;

    virtual std::vector<Node*> computeNodesToCull(
        const Camera& camera,
        const std::vector<std::unique_ptr<Node>>& nodes) = 0;

    void setNodeLoader(NodeLoader* loader);
    NodeLoader* nodeLoader() const;

protected:
    NodeLoader* loader_ = nullptr;
};

#endif //PCLITE_MANAGE_STRATEGY_H
