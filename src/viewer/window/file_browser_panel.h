#ifndef PCLITE_FILE_BROWSER_PANEL_H
#define PCLITE_FILE_BROWSER_PANEL_H

#include <filesystem>
#include <string>

// Minimal ImGui directory/file browser, used in place of a native file
// dialog (no third-party dependency). Owns its current directory across
// frames; call draw() once per frame while the picker should be visible.
class FileBrowserPanel {
public:
    enum class Mode { PickFile, PickDirectory };

    explicit FileBrowserPanel(std::filesystem::path startDir = std::filesystem::current_path());

    // Renders the browser inside the caller's current ImGui window/child.
    // Returns true the frame a selection is confirmed -- double-clicking a
    // file matching extensionFilter in PickFile mode, or pressing "Select
    // This Folder" in PickDirectory mode -- with outPath set to the
    // absolute path. extensionFilter is case-insensitive and includes the
    // leading dot (e.g. ".las"); pass nullptr or "" to show all files.
    bool draw(Mode mode, const char* extensionFilter, std::string& outPath);

private:
    std::filesystem::path currentDir_;
};

#endif //PCLITE_FILE_BROWSER_PANEL_H
