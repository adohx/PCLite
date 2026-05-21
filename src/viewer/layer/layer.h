#ifndef PCLITE_LAYER_H
#define PCLITE_LAYER_H

#include <memory>
#include "node_management/node_manager.h"

class Layer {
public:
    Layer();
    virtual ~Layer() = default;

    // Renders one frame of this layer's content.
    virtual void render() = 0;

    NodeManager& nodeManager();
    const NodeManager& nodeManager() const;

protected:
    std::unique_ptr<NodeManager> nodeManager_;
};

#endif //PCLITE_LAYER_H
