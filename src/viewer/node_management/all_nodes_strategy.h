#ifndef PCLITE_ALL_NODES_STRATEGY_H
#define PCLITE_ALL_NODES_STRATEGY_H

#include "manage_strategy.h"

// Loads every non-proxy node that is not yet loaded; never culls.
class AllNodesStrategy : public ManageStrategy {
public:
    std::vector<Node*> computeNodesToLoad(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) override;

    std::vector<Node*> computeNodesToCull(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) override;
};

#endif //PCLITE_ALL_NODES_STRATEGY_H
