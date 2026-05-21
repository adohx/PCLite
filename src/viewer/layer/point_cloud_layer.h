#ifndef PCLITE_POINT_CLOUD_LAYER_H
#define PCLITE_POINT_CLOUD_LAYER_H

#include "layer.h"

class PointCloudLayer : public Layer {
public:
    // Delegates to nodeManager_->render()
    void render() override;
};

#endif //PCLITE_POINT_CLOUD_LAYER_H
