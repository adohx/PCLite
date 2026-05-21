#include "layer.h"

Layer::Layer() : nodeManager_(std::make_unique<NodeManager>()) {}

NodeManager& Layer::nodeManager() { return *nodeManager_; }
const NodeManager& Layer::nodeManager() const { return *nodeManager_; }
