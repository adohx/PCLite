#ifndef PCLITE_NODE_LOADER_H
#define PCLITE_NODE_LOADER_H

#include <cstdint>
#include <vector>
#include "node.h"

class NodeLoader {
public:
    virtual ~NodeLoader() = default;

    // Returns the raw point data for the given node descriptor.
    // The caller stores the result via Node::setData().
    virtual std::vector<uint8_t> load(const Node& descriptor) = 0;
};

#endif //PCLITE_NODE_LOADER_H
