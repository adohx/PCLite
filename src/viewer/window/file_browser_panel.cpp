#include "file_browser_panel.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool matchesFilter(const fs::path& p, const char* filter) {
    if (!filter || !*filter) return true;
    return toLower(p.extension().string()) == toLower(filter);
}

} // namespace

FileBrowserPanel::FileBrowserPanel(fs::path startDir) : currentDir_(std::move(startDir)) {
    std::error_code ec;
    if (!fs::is_directory(currentDir_, ec)) currentDir_ = fs::current_path();
}

bool FileBrowserPanel::draw(Mode mode, const char* extensionFilter, std::string& outPath) {
    bool selected = false;

    ImGui::TextUnformatted(currentDir_.string().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Up")) {
        fs::path parent = currentDir_.parent_path();
        if (!parent.empty() && parent != currentDir_) currentDir_ = parent;
    }
    if (mode == Mode::PickDirectory) {
        ImGui::SameLine();
        if (ImGui::Button("Select This Folder")) {
            outPath = currentDir_.string();
            selected = true;
        }
    }

    ImGui::Separator();
    ImGui::BeginChild("FileBrowserList", ImVec2(0, 300), true);

    std::error_code ec;
    std::vector<fs::path> dirs, files;
    for (const auto& entry : fs::directory_iterator(currentDir_, ec)) {
        if (entry.is_directory()) dirs.push_back(entry.path());
        else if (mode == Mode::PickFile && matchesFilter(entry.path(), extensionFilter))
            files.push_back(entry.path());
    }
    auto byFilename = [](const fs::path& a, const fs::path& b) { return a.filename() < b.filename(); };
    std::sort(dirs.begin(), dirs.end(), byFilename);
    std::sort(files.begin(), files.end(), byFilename);

    for (const fs::path& d : dirs) {
        std::string label = "[dir] " + d.filename().string();
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            currentDir_ = d;
        }
    }
    for (const fs::path& f : files) {
        std::string label = f.filename().string();
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            outPath = fs::absolute(f).string();
            selected = true;
        }
    }

    ImGui::EndChild();
    return selected;
}
