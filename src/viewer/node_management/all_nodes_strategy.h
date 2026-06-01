#ifndef PCLITE_ALL_NODES_STRATEGY_H
#define PCLITE_ALL_NODES_STRATEGY_H

#include "manage_strategy.h"

// Returns every non-proxy node as active; no culling.
class AllNodesStrategy : public ManageStrategy {
public:
    std::vector<Node*> evaluate(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) override;
};

#endif //PCLITE_ALL_NODES_STRATEGY_H
