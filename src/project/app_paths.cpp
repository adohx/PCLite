#include "app_paths.h"

#include <cstdlib>

namespace app_paths {

std::filesystem::path appDataDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    std::filesystem::path base = appData ? std::filesystem::path(appData) : std::filesystem::current_path();
    return base / "PCLite";
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? std::filesystem::path(home) : std::filesystem::current_path();
    return base / ".pclite";
#endif
}

std::filesystem::path projectsDir() {
    return appDataDir() / "projects";
}

std::filesystem::path recentListPath() {
    return appDataDir() / "recent_projects.json";
}

} // namespace app_paths
