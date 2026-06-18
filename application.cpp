#include "application.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>

#include "converter.h"
#include "window/sdl_window.h"
#include "camera/perspective_camera.h"
#include "camera/arcball_controller.h"
#include "layer/point_cloud_layer.h"
#include "node_management/sse_lru_strategy.h"
#include "node_loader/point_cloud_loader.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"

namespace fs = std::filesystem;

Application::Application(std::string lasPath) : lasPath_(std::move(lasPath)) {}

int Application::run() {
    if (!fs::exists(lasPath_)) {
        fprintf(stderr, "Error: file not found: %s\n", lasPath_.c_str());
        return 1;
    }

    const fs::path outDir = fs::temp_directory_path() / "pclite_out";
    fs::remove_all(outDir);
    fs::create_directories(outDir);

    fprintf(stderr, "Converting %s ...\n", lasPath_.c_str());
    if (!Converter({lasPath_}, outDir.string()).run()) {
        fprintf(stderr, "Conversion failed.\n");
        fs::remove_all(outDir);
        return 1;
    }

    //return 0;
    auto loader = std::make_unique<PointCloudLoader>(outDir.string());
    auto root   = loader->loadRoot();

    auto bb    = root->tightBB_;
    auto bbMin = bb.min();
    auto bbMax = bb.max();
    float span = (float)std::max({bbMax.x - bbMin.x,
                                  bbMax.y - bbMin.y,
                                  bbMax.z - bbMin.z});

    fprintf(stderr, "BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]  span=%.1fm\n",
            bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, span);

    auto layer    = std::make_unique<PointCloudLayer>();
    auto strategy = std::make_unique<SSELruStrategy>(/*screenHeight=*/768,
                                                        /*sseThreshold=*/1.0f,
                                                        /*maxLoadedNodes=*/1000);
    strategy->setNodeLoader(loader.get());

    auto& mgr = layer->nodeManager();
    mgr.addTree(root);
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(loader->attributes()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());

    vec3d center = {(bbMin.x + bbMax.x) * 0.5,
                    (bbMin.y + bbMax.y) * 0.5,
                    (bbMin.z + bbMax.z) * 0.5};

    auto camera = std::make_unique<PerspectiveCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(bb);

    auto controller = std::make_unique<ArcballController>();
    controller->syncFromCamera(*camera);

    SDLWindow window(1024, 768, "PCLite - " + lasPath_);
    window.addLayer(std::move(layer));
    window.addCamera(std::move(camera));
    window.setController(std::move(controller));
    window.run();

    fs::remove_all(outDir);
    return 0;
}
