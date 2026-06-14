// PCLite point cloud viewer
// Usage: pcl_viewer <dataset_dir>

#include <cstdio>
#include <algorithm>
#include <memory>

#include "window/sdl_window.h"
#include "camera/perspective_camera.h"
#include "camera/arcball_controller.h"
#include "layer/point_cloud_layer.h"
#include "node_management/sse_lru_strategy.h"
#include "node_loader/point_cloud_loader.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"
#include "../src/core/node.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pcl_viewer <dataset_dir>\n");
        return 1;
    }

    auto dataset = std::make_unique<PointCloudLoader>(argv[1]);
    auto root    = dataset->loadRoot();

    auto bb    = root->tightBB_;
    auto bbMin = bb.min();
    auto bbMax = bb.max();
    float span = (float)std::max({bbMax.x - bbMin.x,
                                  bbMax.y - bbMin.y,
                                  bbMax.z - bbMin.z});

    printf("BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]  span=%.1fm\n",
        bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, span);

    auto layer    = std::make_unique<PointCloudLayer>();
    auto strategy = std::make_unique<SSELruStrategy>(/*screenHeight=*/800,
                                                        /*sseThreshold=*/1.0f,
                                                        /*maxLoadedNodes=*/1000);
    strategy->setNodeLoader(dataset.get());

    auto& mgr = layer->nodeManager();
    mgr.addTree(root);
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(dataset->attributes()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());

    vec3d center = {
        (bbMin.x + bbMax.x) * 0.5,
        (bbMin.y + bbMax.y) * 0.5,
        (bbMin.z + bbMax.z) * 0.5,
    };
    auto camera = std::make_unique<PerspectiveCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(bb);

    auto controller = std::make_unique<ArcballController>();
    controller->syncFromCamera(*camera);

    SDLWindow window(800, 600, "PCLite Viewer");
    window.addLayer(std::move(layer));
    window.addCamera(std::move(camera));
    window.setController(std::move(controller));
    window.run();

    return 0;
}
