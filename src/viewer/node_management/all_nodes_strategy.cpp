#include "all_nodes_strategy.h"
#include "../../core/node.h"

std::vector<Node*> AllNodesStrategy::evaluate(
    const Camera&,
    const std::vector<std::shared_ptr<Node>>& nodes)
{
    std::vector<Node*> active;
    for (auto& n : nodes)
        if (n->type_ != NodeType::Proxy)
            active.push_back(n.get());
    return active;
}
