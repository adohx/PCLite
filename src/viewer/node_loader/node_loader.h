#ifndef PCLITE_NODE_LOADER_H
#define PCLITE_NODE_LOADER_H

#include <functional>
#include <vector>
#include <cstdint>
#include "../../core/node.h"

template <typename T>
class NodeLoader {
public:
    virtual ~NodeLoader() = default;

    virtual bool load(std::shared_ptr<T> node) = 0;
    virtual std::shared_ptr<T> loadRoot() =0;

};

#endif //PCLITE_NODE_LOADER_H
