// PCLite point cloud viewer
// Usage: pcl_viewer <dataset_dir>

#include <cstdio>
#include <algorithm>
#include <memory>

#include "window/sdl_window.h"
#include "camera/orbit_camera.h"
#include "layer/point_cloud_layer.h"
#include "node_management/all_nodes_strategy.h"
#include "node_loader/file_node_loader.h"
#include "node_loader/point_cloud_loader.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"
#include "node.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pcl_viewer <dataset_dir>\n");
        return 1;
    }

    // 1. Load dataset (metadata + hierarchy)
    PointCloudLoader dataset(argv[1]);
    auto nodes = dataset.takeNodes();

    auto bbox = [&](auto fn) {
        return std::max({ fn(dataset.bboxMax().x - dataset.bboxMin().x),
                          fn(dataset.bboxMax().y - dataset.bboxMin().y),
                          fn(dataset.bboxMax().z - dataset.bboxMin().z) });
    };
    float span = (float)bbox([](double v){ return v; });

    printf("BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]  span=%.1fm\n",
        dataset.bboxMin().x, dataset.bboxMin().y, dataset.bboxMin().z,
        dataset.bboxMax().x, dataset.bboxMax().y, dataset.bboxMax().z, span);

    // Cap to a point budget
    const uint64_t POINT_BUDGET = 2'000'000;
    uint64_t total = 0;
    size_t keep = 0;
    for (auto& n : nodes) {
        if (n->type() == NodeType::Proxy) { ++keep; continue; }
        total += n->pointCount();
        ++keep;
        if (total >= POINT_BUDGET) break;
    }
    nodes.resize(keep);
    printf("Loading %zu nodes (~%llu pts)\n", keep, (unsigned long long)total);

    // 2. Build layer
    auto layer    = std::make_unique<PointCloudLayer>();
    auto loader   = std::make_unique<FileNodeLoader>(std::string(argv[1]) + "/octree.bin");
    auto strategy = std::make_unique<AllNodesStrategy>();
    strategy->setNodeLoader(loader.get());

    auto& mgr = layer->nodeManager();
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(dataset.layout()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    for (auto& n : nodes)
        mgr.addNode(std::move(n));

    // 3. Build camera
    vec3d center = {
        (dataset.bboxMin().x + dataset.bboxMax().x) * 0.5,
        (dataset.bboxMin().y + dataset.bboxMax().y) * 0.5,
        (dataset.bboxMin().z + dataset.bboxMax().z) * 0.5,
    };
    auto camera = std::make_unique<OrbitCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(BoundaryBoxd(dataset.bboxMin(), dataset.bboxMax()));

    // AxisPainter holds a raw pointer — add before moving camera into window
    mgr.addPainter(std::make_unique<AxisPainter>());

    // 4. Run
    SDLWindow window(1280, 800, "PCLite Viewer");
    window.addCamera(std::move(camera));
    window.addLayer(std::move(layer));
    window.syncOrbitFromCamera();  // sync distance/azimuth/elevation from lookAt result
    window.run();

    return 0;
}
