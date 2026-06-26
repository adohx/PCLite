#include "project_hub_panel.h"
#include "imgui.h"

#include <chrono>
#include <cstring>
#include <filesystem>

ProjectHubPanel::ProjectHubPanel(ProjectManager& projectManager)
    : projectManager_(projectManager) {}

void ProjectHubPanel::setOnOpenProject(std::function<void(const ProjectInfo&)> cb) {
    onOpenProject_ = std::move(cb);
}

void ProjectHubPanel::setImportFn(ImportFn fn) {
    importFn_ = std::move(fn);
}

const char* ProjectHubPanel::stageLabel(ImportStage stage) {
    switch (stage) {
        case ImportStage::Chunking:   return "Chunking points...";
        case ImportStage::Hierarchy:  return "Building hierarchy...";
        case ImportStage::Sampling:   return "Sampling octree levels...";
        case ImportStage::Merging:    return "Merging chunks...";
        case ImportStage::Rebuilding: return "Rebuilding index...";
        default:                      return "Starting...";
    }
}

void ProjectHubPanel::startImport(const ProjectInfo& info) {
    pendingProjectInfo_ = info;
    importing_.store(true);
    importFraction_.store(0.f);
    importStage_.store(ImportStage::Chunking);
    importError_.clear();

    ImportFn importFn = importFn_;
    std::string lasPath = pickedLasPath_;
    std::string targetDir = info.path;

    importFuture_ = std::async(std::launch::async, [this, importFn, lasPath, targetDir]() {
        return importFn(lasPath, targetDir, [this](const std::string& stage, float fraction) {
            importFraction_.store(fraction);
            ImportStage s = ImportStage::None;
            if (stage == "chunking") s = ImportStage::Chunking;
            else if (stage == "hierarchy") s = ImportStage::Hierarchy;
            else if (stage == "sampling") s = ImportStage::Sampling;
            else if (stage == "merging") s = ImportStage::Merging;
            else if (stage == "rebuilding") s = ImportStage::Rebuilding;
            importStage_.store(s);
        });
    });

    pickedLasPath_.clear();
    nameBuffer_[0] = '\0';
}

void ProjectHubPanel::pollImport() {
    if (!importing_.load()) return;
    if (!importFuture_.valid()) return;
    if (importFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    bool ok = importFuture_.get();
    importing_.store(false);

    if (ok) {
        projectManager_.touchRecent(pendingProjectInfo_);
        if (onOpenProject_) onOpenProject_(pendingProjectInfo_);
    } else {
        importError_ = "Import failed for \"" + pendingProjectInfo_.name + "\".";
        projectManager_.deleteProject(pendingProjectInfo_); // clean up the half-created directory
    }
}

void ProjectHubPanel::drawErrorBanner() {
    if (importError_.empty()) return;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
    ImGui::TextUnformatted(importError_.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::SmallButton("Dismiss")) importError_.clear();
    ImGui::Separator();
}

void ProjectHubPanel::draw() {
    pollImport();

    ImGui::TextUnformatted("PCLite Projects");
    ImGui::Spacing();
    drawErrorBanner();

    ImGui::BeginDisabled(importing_.load());
    if (ImGui::Button("New Project")) {
        pickedLasPath_.clear();
        nameBuffer_[0] = '\0';
        ImGui::OpenPopup("NewProjectWizard");
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal("NewProjectWizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        drawNewProjectPopupContent();
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    drawRecentSection();
    ImGui::Spacing();
    drawAllProjectsSection();

    if (deleteRequested_) {
        deleteRequested_ = false;
        ImGui::OpenPopup("DeleteProjectConfirm"); // called here, outside any row's PushID scope
    }
    drawDeleteConfirmPopup();
}

void ProjectHubPanel::drawNewProjectPopupContent() {
    if (importing_.load()) {
        ImGui::Text("%s", stageLabel(importStage_.load()));
        ImGui::ProgressBar(importFraction_.load(), ImVec2(300, 0));
        ImGui::TextDisabled("This can take a while for large LAS files.");
        return; // no cancel for v1; the popup just sits here until done
    }

    if (pickedLasPath_.empty()) {
        ImGui::TextUnformatted("Pick a LAS file to import:");
        std::string picked;
        if (fileBrowser_.draw(FileBrowserPanel::Mode::PickFile, ".las", picked)) {
            pickedLasPath_ = picked;
            std::filesystem::path p(picked);
            std::string stem = p.stem().string();
            std::strncpy(nameBuffer_, stem.c_str(), sizeof(nameBuffer_) - 1);
        }
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        return;
    }

    ImGui::Text("Selected: %s", pickedLasPath_.c_str());
    if (ImGui::Button("Change File")) {
        pickedLasPath_.clear();
        return;
    }

    ImGui::InputText("Project Name", nameBuffer_, sizeof(nameBuffer_));

    bool canCreate = nameBuffer_[0] != '\0';
    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create")) {
        ProjectInfo info = projectManager_.createProject(nameBuffer_, pickedLasPath_);
        startImport(info);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
}

void ProjectHubPanel::drawRecentSection() {
    ImGui::TextUnformatted("Recent");
    auto recent = projectManager_.recentProjects();
    if (recent.empty()) {
        ImGui::TextDisabled("(none yet)");
        return;
    }
    for (const ProjectInfo& info : recent) drawProjectRow(info, "recent");
}

void ProjectHubPanel::drawAllProjectsSection() {
    ImGui::TextUnformatted("All Projects");
    for (const ProjectInfo& info : projectManager_.listProjects()) {
        // Don't show the project currently being imported -- its dataset
        // isn't complete yet, so opening it would fail.
        if (importing_.load() && info.path == pendingProjectInfo_.path) continue;
        drawProjectRow(info, "all");
    }
}

void ProjectHubPanel::drawProjectRow(const ProjectInfo& info, const char* idScope) {
    ImGui::PushID(idScope);
    ImGui::PushID(info.path.c_str());

    if (ImGui::Button("Open")) {
        projectManager_.touchRecent(info);
        if (onOpenProject_) onOpenProject_(info);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        pendingDeletePath_ = info.path;
        pendingDeleteName_ = info.name;
        deleteRequested_ = true; // OpenPopup() is called from draw(), outside this PushID scope
    }
    ImGui::SameLine();
    ImGui::Text("%s  (%s, %s)", info.name.c_str(), info.sourceFile.c_str(), info.createdAt.c_str());

    ImGui::PopID();
    ImGui::PopID();
}

void ProjectHubPanel::drawDeleteConfirmPopup() {
    if (ImGui::BeginPopupModal("DeleteProjectConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete \"%s\"? This cannot be undone.", pendingDeleteName_.c_str());
        if (ImGui::Button("Delete")) {
            ProjectInfo info;
            info.path = pendingDeletePath_;
            projectManager_.deleteProject(info);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
