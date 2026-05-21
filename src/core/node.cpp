#include "node.h"

Node::Node(NodeType type, uint8_t childMask, uint32_t pointCount,
           uint64_t address, uint64_t byteSize, vec3d min, vec3d max)
    : type_(type), childMask_(childMask), pointCount_(pointCount),
      address_(address), byteSize_(byteSize), min_(min), max_(max) {}

Node::~Node() = default;

NodeType Node::type() const { return type_; }
uint8_t  Node::childMask() const { return childMask_; }
uint32_t Node::pointCount() const { return pointCount_; }
uint64_t Node::address() const { return address_; }
uint64_t Node::byteSize() const { return byteSize_; }
vec3d    Node::min() const { return min_; }
vec3d    Node::max() const { return max_; }

bool Node::isLoaded() const { return !data_.empty(); }
const std::vector<uint8_t>& Node::data() const { return data_; }

void Node::setData(std::vector<uint8_t> data) {
    data_ = std::move(data);
}

void Node::clearData() {
    data_.clear();
    data_.shrink_to_fit();
}
