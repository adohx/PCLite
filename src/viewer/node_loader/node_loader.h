#ifndef PCLITE_NODE_LOADER_H
#define PCLITE_NODE_LOADER_H

#include <functional>
#include <vector>
#include <cstdint>
#include "node.h"

// Base loader: stores a callback and a target node.
// Subclasses call bindCallback() before use, setTarget() before each load().
class NodeLoader {
public:
    using DataCallback = std::function<void(std::vector<uint8_t>)>;

    virtual ~NodeLoader() = default;

    // Set what to do with the loaded bytes (call once or per-node).
    void bindCallback(DataCallback cb) { callback_ = std::move(cb); }

    // Tell the loader which node to load next.
    virtual void setTarget(Node& node) = 0;

    // Load the target set by setTarget() and deliver via callback_.
    virtual void load() = 0;

protected:
    DataCallback callback_;
    Node*        target_ = nullptr;
};

#endif //PCLITE_NODE_LOADER_H
