// PCLite point cloud viewer
// Usage: pcl_viewer <dataset_dir>

#include <cstdio>
#include <algorithm>
#include <memory>

#include "window/sdl_window.h"
#include "camera/orbit_camera.h"
#include "layer/point_cloud_layer.h"
#include "node_management/all_nodes_strategy.h"
#include "node_loader/point_cloud_loader.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"
#include "../src/viewer/node_loader/node.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pcl_viewer <dataset_dir>\n");
        return 1;
    }

    auto dataset = std::make_unique<PointCloudLoader>(argv[1]);
    auto root    = dataset->loadRoot();

    auto bb    = dataset->boundingBox();
    auto bbMin = bb.min();
    auto bbMax = bb.max();
    float span = (float)std::max({bbMax.x - bbMin.x,
                                  bbMax.y - bbMin.y,
                                  bbMax.z - bbMin.z});

    printf("BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]  span=%.1fm\n",
        bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, span);

    auto layer    = std::make_unique<PointCloudLayer>();
    auto strategy = std::make_unique<AllNodesStrategy>();
    strategy->setNodeLoader(dataset.get());

    auto& mgr = layer->nodeManager();
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(dataset->attributes()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());

    vec3d center = {
        (bbMin.x + bbMax.x) * 0.5,
        (bbMin.y + bbMax.y) * 0.5,
        (bbMin.z + bbMax.z) * 0.5,
    };
    auto camera = std::make_unique<OrbitCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(bb);

    SDLWindow window(1280, 800, "PCLite Viewer");
    window.addCamera(std::move(camera));
    window.addLayer(std::move(layer));
    window.syncOrbitFromCamera();
    window.run();

    return 0;
}
