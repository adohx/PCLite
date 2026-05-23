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
#include "node.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pcl_viewer <dataset_dir>\n");
        return 1;
    }

    // 1. Parse metadata; hierarchy loaded on demand via loadHierarchy()
    auto dataset = std::make_unique<PointCloudLoader>(argv[1]);
    auto nodes   = dataset->loadHierarchy();

    float span = (float)std::max({
        dataset->bboxMax().x - dataset->bboxMin().x,
        dataset->bboxMax().y - dataset->bboxMin().y,
        dataset->bboxMax().z - dataset->bboxMin().z });

    printf("BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]  span=%.1fm\n",
        dataset->bboxMin().x, dataset->bboxMin().y, dataset->bboxMin().z,
        dataset->bboxMax().x, dataset->bboxMax().y, dataset->bboxMax().z, span);

    // Cap to a point budget
    const uint64_t POINT_BUDGET = 2'000'000;
    uint64_t total = 0;
    size_t keep = 0;
    for (auto& n : nodes) {
        if (n->type == NodeType::Proxy) { ++keep; continue; }
        total += n->pointCount;
        ++keep;
        if (total >= POINT_BUDGET) break;
    }
    nodes.resize(keep);
    printf("Loading %zu nodes (~%llu pts)\n", keep, (unsigned long long)total);

    // 2. Build layer — dataset itself serves as NodeLoader
    auto layer    = std::make_unique<PointCloudLayer>();
    auto strategy = std::make_unique<AllNodesStrategy>();
    strategy->setNodeLoader(dataset.get());   // PointCloudLoader implements NodeLoader

    auto& mgr = layer->nodeManager();
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(dataset->attributes()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());
    for (auto& n : nodes)
        mgr.addNode(std::move(n));

    // 3. Build camera
    vec3d center = {
        (dataset->bboxMin().x + dataset->bboxMax().x) * 0.5,
        (dataset->bboxMin().y + dataset->bboxMax().y) * 0.5,
        (dataset->bboxMin().z + dataset->bboxMax().z) * 0.5,
    };
    auto camera = std::make_unique<OrbitCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(BoundingBoxd(dataset->bboxMin(), dataset->bboxMax()));

    // 4. Run  (dataset must outlive window — strategy holds a raw ptr to it)
    SDLWindow window(1280, 800, "PCLite Viewer");
    window.addCamera(std::move(camera));
    window.addLayer(std::move(layer));
    window.syncOrbitFromCamera();
    window.run();

    return 0;
}
