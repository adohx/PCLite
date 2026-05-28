#include "manage_strategy.h"

void ManageStrategy::setNodeLoader(NodeLoader<Node>* loader) { loader_ = loader; }
NodeLoader<Node>* ManageStrategy::nodeLoader() const { return loader_; }
