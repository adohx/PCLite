#include "all_nodes_strategy.h"
#include "../node_loader/node.h"

std::vector<Node*> AllNodesStrategy::computeNodesToLoad(
    const Camera&,
    const std::vector<std::unique_ptr<Node>>& nodes)
{
    std::vector<Node*> toLoad;
    for (auto& n : nodes)
        if (!n->isLoaded() && n->type_ != NodeType::Proxy)
            toLoad.push_back(n.get());
    return toLoad;
}

std::vector<Node*> AllNodesStrategy::computeNodesToCull(
    const Camera&,
    const std::vector<std::unique_ptr<Node>>&)
{
    return {};
}
