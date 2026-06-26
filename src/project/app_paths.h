#ifndef PCLITE_APP_PATHS_H
#define PCLITE_APP_PATHS_H

#include <filesystem>

// Where PCLite stores its own persistent state (projects + the recent-projects
// list), as opposed to user-chosen file locations. Kept separate from
// ProjectManager so the latter can be pointed at a temp directory in tests
// without touching the real per-user location.
namespace app_paths {

// $HOME/.pclite on Linux/macOS, %APPDATA%/PCLite on Windows.
std::filesystem::path appDataDir();

std::filesystem::path projectsDir();
std::filesystem::path recentListPath();

} // namespace app_paths

#endif //PCLITE_APP_PATHS_H
