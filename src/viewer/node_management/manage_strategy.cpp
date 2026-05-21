#include "manage_strategy.h"

void ManageStrategy::setNodeLoader(NodeLoader* loader) { loader_ = loader; }
NodeLoader* ManageStrategy::nodeLoader() const { return loader_; }
