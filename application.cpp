#include "application.h"

#include "imgui.h"

#include <algorithm>

#include "converter.h"
#include "camera/perspective_camera.h"
#include "camera/arcball_controller.h"
#include "layer/point_cloud_layer.h"
#include "measurement/measurement_manager.h"
#include "node_management/sse_lru_strategy.h"
#include "painter/node_painter.h"
#include "painter/bounding_box_painter.h"
#include "painter/axis_painter.h"
#include "painter/rotation_center_painter.h"
#include "painter/plane_fit_ring_painter.h"
#include "painter/measurement_painter.h"

#include <string>

namespace {
// Thousands-separated decimal string (e.g. 44026810 -> "44,026,810"), for
// the point-count stats -- plain %llu is unreadable at point-cloud scale.
std::string formatWithCommas(uint64_t value) {
    std::string s = std::to_string(value);
    for (int pos = (int)s.length() - 3; pos > 0; pos -= 3)
        s.insert(pos, ",");
    return s;
}
}

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
    mainWindow_.setPropertiesCallback([this] { drawProperties(); });
    mainWindow_.setToolbarCallback([this] { drawToolbar(); });
    mainWindow_.setMode(MainWindow::Mode::Hub);
}

void Application::drawFileMenu() {
    bool projectOpen = mainWindow_.mode() == MainWindow::Mode::Project;
    if (ImGui::MenuItem("Close Project", nullptr, false, projectOpen)) closeProject();
}

void Application::drawProperties() {
    if (ImGui::SliderFloat("Point size", &pointSize_, 1.f, 10.f))
        if (currentNodePainter_) currentNodePainter_->setPointSize(pointSize_);

    if (currentLoader_) ImGui::Separator();
    if (currentLoader_ && ImGui::BeginTable("PointStats", 2)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Total points");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(formatWithCommas(currentLoader_->totalPoints()).c_str());

        if (currentNodePainter_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Displayed points");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(formatWithCommas(currentNodePainter_->visiblePointCount()).c_str());
        }
        ImGui::EndTable();
    }
}

void Application::drawToolbar() {
    if (mainWindow_.mode() != MainWindow::Mode::Project) return;

    // Mode-select button; no icon font is loaded yet, so these are plain
    // text labels for now -- swap for icon glyphs if/when one is added.
    auto modeButton = [&](const char* label, bool active, auto onClick) {
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label)) onClick();
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    auto& mm = mainWindow_.viewport().measurementManager();
    ImGui::TextUnformatted("Measure:");
    ImGui::SameLine();
    modeButton("None", mm.mode() == MeasurementMode::None,
               [&] { mm.setMode(MeasurementMode::None); });
    modeButton("Distance", mm.mode() == MeasurementMode::Distance,
               [&] { mm.setMode(MeasurementMode::Distance); });
    modeButton("Angle", mm.mode() == MeasurementMode::Angle,
               [&] { mm.setMode(MeasurementMode::Angle); });
    if (mm.mode() != MeasurementMode::None && ImGui::Button("Clear"))
        mm.clear();

    ImGui::SameLine();
    ImGui::TextUnformatted("  |  Rotate Around:");
    ImGui::SameLine();
    auto& viewport = mainWindow_.viewport();
    modeButton("Fixed", viewport.rotationCenterMode() == RotationCenterMode::Fixed,
               [&] { viewport.setRotationCenterMode(RotationCenterMode::Fixed); });
    modeButton("Double-Click", viewport.rotationCenterMode() == RotationCenterMode::DoubleClick,
               [&] { viewport.setRotationCenterMode(RotationCenterMode::DoubleClick); });
    modeButton("Follow", viewport.rotationCenterMode() == RotationCenterMode::Follow,
               [&] { viewport.setRotationCenterMode(RotationCenterMode::Follow); });
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

    auto nodePainter = std::make_unique<NodePainter>(currentLoader_->attributes());
    nodePainter->setPointSize(pointSize_);
    currentNodePainter_ = nodePainter.get();
    mgr.addPainter(std::move(nodePainter));

    mgr.addPainter(std::make_unique<BoundingBoxPainter>());
    mgr.addPainter(std::make_unique<AxisPainter>());
    mgr.addPainter(std::make_unique<RotationCenterPainter>());
    mgr.addPainter(std::make_unique<PlaneFitRingPainter>());
    mgr.addPainter(std::make_unique<MeasurementPainter>(&mainWindow_.viewport().measurementManager()));

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
    currentNodePainter_ = nullptr;
    mainWindow_.setMode(MainWindow::Mode::Hub);
}

int Application::run() {
    mainWindow_.run();
    return 0;
}
