#include "application.h"

#include "imgui.h"
#include <glad/gl.h>

#include <algorithm>

#include "converter.h"
#include "camera/perspective_camera.h"
#include "camera/arcball_controller.h"
#include "layer/point_cloud_layer.h"
#include "node_management/sse_lru_strategy.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"
#include "painter/rotation_center_painter.h"
#include "painter/plane_fit_ring_painter.h"

Application::Application()
    : mainWindow_(1280, 800, "PCLite"),
      hubPanel_(projectManager_) {
    hubPanel_.setImportFn([](const std::string& lasPath, const std::string& targetDir,
                              std::function<void(const std::string&, float)> onProgress) {
        Converter converter({lasPath}, targetDir);
        converter.setProgressCallback(std::move(onProgress));
        return converter.run();
    });
    hubPanel_.setOnOpenProject([this](const ProjectInfo& info) { openProject(info); });

    mainWindow_.setHubContentCallback([this] { hubPanel_.draw(); });
    mainWindow_.setFileMenuCallback([this] { drawFileMenu(); });
    mainWindow_.setPropertiesCallback([this] {
        if (ImGui::SliderFloat("Point size", &pointSize_, 1.f, 10.f)) glPointSize(pointSize_);
    });
    mainWindow_.setMode(MainWindow::Mode::Hub);
}

void Application::drawFileMenu() {
    bool projectOpen = mainWindow_.mode() == MainWindow::Mode::Project;
    if (ImGui::MenuItem("Close Project", nullptr, false, projectOpen)) closeProject();
}

void Application::openProject(const ProjectInfo& info) {
    currentLoader_ = std::make_unique<PointCloudLoader>(info.path);
    auto root = currentLoader_->loadRoot();

    auto bb = root->tightBB_;
    auto bbMin = bb.min();
    auto bbMax = bb.max();
    float span = (float)std::max({bbMax.x - bbMin.x, bbMax.y - bbMin.y, bbMax.z - bbMin.z});

    auto layer = std::make_unique<PointCloudLayer>();
    auto strategy = std::make_unique<SSELruStrategy>(/*screenHeight=*/768,
                                                       /*sseThreshold=*/1.0f,
                                                       /*maxLoadedNodes=*/1000);
    strategy->setNodeLoader(currentLoader_.get());

    auto& mgr = layer->nodeManager();
    mgr.addTree(root);
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::make_unique<NodePainter>(currentLoader_->attributes()));
    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());
    mgr.addPainter(std::make_unique<RotationCenterPainter>());
    mgr.addPainter(std::make_unique<PlaneFitRingPainter>());

    vec3d center = {(bbMin.x + bbMax.x) * 0.5, (bbMin.y + bbMax.y) * 0.5, (bbMin.z + bbMax.z) * 0.5};

    auto camera = std::make_unique<PerspectiveCamera>();
    camera->setTarget(center);
    camera->setNearPlane(span * 0.001f);
    camera->setFarPlane(span * 10.f);
    camera->lookAt(bb);

    auto controller = std::make_unique<ArcballController>();
    controller->syncFromCamera(*camera);

    mainWindow_.viewport().addLayer(std::move(layer));
    mainWindow_.viewport().addCamera(std::move(camera));
    mainWindow_.viewport().setController(std::move(controller));
    mainWindow_.viewport().setPickAssistLoader(currentLoader_.get());

    mainWindow_.setMode(MainWindow::Mode::Project);
}

void Application::closeProject() {
    mainWindow_.viewport().reset(); // tears down everything that references currentLoader_ first
    currentLoader_.reset();
    mainWindow_.setMode(MainWindow::Mode::Hub);
}

int Application::run() {
    mainWindow_.run();
    return 0;
}
