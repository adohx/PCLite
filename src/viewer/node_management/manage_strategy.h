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

    // Returns the set of nodes that should be active (loaded and rendered) this
    // frame.  NodeManager loads any nodes in the set that are not yet loaded,
    // and culls any nodes previously active but absent from the new set.
    virtual std::vector<Node*> evaluate(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) = 0;

    void setNodeLoader(NodeLoader<Node>* loader);
    NodeLoader<Node>* nodeLoader() const;

protected:
    NodeLoader<Node>* loader_ = nullptr;
};

#endif //PCLITE_MANAGE_STRATEGY_H
