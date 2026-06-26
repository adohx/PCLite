#ifndef PCLITE_PROJECT_HUB_PANEL_H
#define PCLITE_PROJECT_HUB_PANEL_H

#include "file_browser_panel.h"
#include "project_manager.h"

#include <atomic>
#include <functional>
#include <future>
#include <string>

// The "no project open" screen (shown by MainWindow in Hub mode): recent
// list, full project list (open/delete), and a "New Project" wizard (pick a
// LAS file -> name it -> import). Owns no GL/Viewport state -- it only ever
// asks the app to actually open/import a project via callbacks; the
// Converter/PointCloudLoader/Layer wiring lives in Application.
class ProjectHubPanel {
public:
    explicit ProjectHubPanel(ProjectManager& projectManager);

    // Called once per frame while MainWindow is in Hub mode.
    void draw();

    // Invoked the frame the user picks a project to open (from Recent, the
    // full list, or right after a successful New Project import).
    void setOnOpenProject(std::function<void(const ProjectInfo&)> cb);

    // Actually runs a conversion (sourceLasPath -> targetDir), reporting
    // progress via the third argument exactly like Converter does. Supplied
    // by Application (which owns the Converter dependency) so this panel
    // stays decoupled from it. Run via std::async -- the callback may fire
    // on a background thread; draw() only ever reads the atomics it updates.
    using ImportFn = std::function<bool(const std::string& lasPath, const std::string& targetDir,
                                         std::function<void(const std::string&, float)> onProgress)>;
    void setImportFn(ImportFn fn);

private:
    enum class ImportStage { None, Chunking, Hierarchy, Sampling, Merging, Rebuilding };

    ProjectManager& projectManager_;
    FileBrowserPanel fileBrowser_;
    std::function<void(const ProjectInfo&)> onOpenProject_;
    ImportFn importFn_;

    std::string pickedLasPath_;
    char nameBuffer_[256] = {};

    std::future<bool> importFuture_;
    std::atomic<float> importFraction_{0.f};
    std::atomic<ImportStage> importStage_{ImportStage::None};
    std::atomic<bool> importing_{false};
    ProjectInfo pendingProjectInfo_;
    std::string importError_;

    std::string pendingDeletePath_;
    std::string pendingDeleteName_;
    bool deleteRequested_ = false;

    void drawErrorBanner();
    void drawRecentSection();
    void drawAllProjectsSection();
    // idScope disambiguates ImGui IDs when the same project appears in both
    // the Recent and All Projects sections (e.g. "recent" vs "all") -- their
    // widgets would otherwise collide since both are keyed by info.path.
    void drawProjectRow(const ProjectInfo& info, const char* idScope);
    void drawNewProjectPopupContent();
    void drawDeleteConfirmPopup();
    void startImport(const ProjectInfo& info);
    void pollImport();
    static const char* stageLabel(ImportStage stage);
};

#endif //PCLITE_PROJECT_HUB_PANEL_H
