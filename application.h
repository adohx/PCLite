#pragma once

#include "node_loader/point_cloud_loader.h"
#include "project_manager.h"
#include "window/main_window.h"
#include "window/project_hub_panel.h"

#include <memory>

// Top-level orchestrator: owns the persistent MainWindow/GL context and the
// ProjectManager, and switches MainWindow between Hub mode (no project open
// -- ProjectHubPanel drives create/open/delete/recent) and Project mode
// (today's Toolbar/Status/Properties/Viewport layout) at runtime.
class Application {
public:
    Application();
    int run();

private:
    void openProject(const ProjectInfo& info);
    void closeProject();
    void drawFileMenu();

    ProjectManager projectManager_;
    MainWindow mainWindow_;
    ProjectHubPanel hubPanel_;

    // Owns the loader for whichever project is currently open; null in Hub
    // mode. Destroyed (in closeProject()) only after Viewport::reset() has
    // already torn down everything that references it.
    std::unique_ptr<PointCloudLoader> currentLoader_;

    float pointSize_ = 2.f;
};
