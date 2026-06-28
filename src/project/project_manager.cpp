#include "project_manager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::string sanitizeSlug(const std::string& name) {
    std::string slug;
    slug.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == ' ')
            slug.push_back(c == ' ' ? '_' : c);
    }
    if (slug.empty()) slug = "project";
    if (slug.size() > 64) slug.resize(64);
    return slug;
}

std::string nowDisplayString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

} // namespace

bool loadProjectInfo(const fs::path& dir, ProjectInfo& info) {
    fs::path jsonPath = dir / "project.json";
    std::ifstream in(jsonPath);
    if (!in.is_open()) return false;

    json j;
    try {
        in >> j;
    } catch (const json::exception&) {
        return false;
    }

    info.name = j.value("name", dir.filename().string());
    info.path = dir.string();
    info.sourceFile = j.value("sourceFile", std::string());
    info.createdAt = j.value("createdAt", std::string());
    return true;
}

ProjectManager::ProjectManager(fs::path projectsDir, fs::path recentListPath)
    : projectsDir_(std::move(projectsDir)), recentListPath_(std::move(recentListPath)) {}

std::vector<ProjectInfo> ProjectManager::listProjects() const {
    std::vector<ProjectInfo> result;
    std::error_code ec;
    if (!fs::exists(projectsDir_, ec)) return result;

    for (const auto& entry : fs::directory_iterator(projectsDir_, ec)) {
        if (!entry.is_directory()) continue;
        ProjectInfo info;
        if (loadProjectInfo(entry.path(), info)) result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.createdAt > b.createdAt; // "YYYY-MM-DD HH:MM" sorts lexicographically = chronologically
    });
    return result;
}

std::vector<std::string> ProjectManager::readRecentPaths() const {
    std::ifstream in(recentListPath_);
    if (!in.is_open()) return {};

    json j;
    try {
        in >> j;
    } catch (const json::exception&) {
        return {};
    }
    if (!j.is_array()) return {};

    std::vector<std::string> paths;
    for (const auto& item : j)
        if (item.is_string()) paths.push_back(item.get<std::string>());
    return paths;
}

void ProjectManager::writeRecentPaths(const std::vector<std::string>& paths) const {
    std::error_code ec;
    fs::create_directories(recentListPath_.parent_path(), ec);

    std::ofstream out(recentListPath_);
    if (!out.is_open()) return;
    out << json(paths).dump(2);
}

std::vector<ProjectInfo> ProjectManager::recentProjects() const {
    std::vector<std::string> raw = readRecentPaths();
    std::vector<ProjectInfo> result;
    std::vector<std::string> stillValid;

    for (const std::string& p : raw) {
        ProjectInfo info;
        if (loadProjectInfo(p, info)) {
            result.push_back(std::move(info));
            stillValid.push_back(p);
        }
    }

    if (stillValid.size() != raw.size()) writeRecentPaths(stillValid); // self-heal stale entries
    return result;
}

ProjectInfo ProjectManager::createProject(const std::string& name, const std::string& sourceLasPath) const {
    std::string baseSlug = sanitizeSlug(name);
    std::string slug = baseSlug;
    std::error_code ec;
    for (int suffix = 2; fs::exists(projectsDir_ / slug, ec); ++suffix)
        slug = baseSlug + "-" + std::to_string(suffix);

    fs::path dir = projectsDir_ / slug;
    fs::create_directories(dir, ec);

    ProjectInfo info;
    info.name = name;
    info.path = dir.string();
    info.sourceFile = sourceLasPath;
    info.createdAt = nowDisplayString();

    json j;
    j["version"] = 1;
    j["name"] = info.name;
    j["sourceFile"] = info.sourceFile;
    j["createdAt"] = info.createdAt;

    std::ofstream out(dir / "project.json");
    out << j.dump(2);

    return info;
}

bool ProjectManager::deleteProject(const ProjectInfo& info) const {
    std::error_code ec;
    fs::remove_all(info.path, ec);

    std::vector<std::string> remaining;
    for (const std::string& p : readRecentPaths())
        if (p != info.path) remaining.push_back(p);
    writeRecentPaths(remaining);

    return !fs::exists(info.path, ec);
}

void ProjectManager::touchRecent(const ProjectInfo& info) const {
    std::vector<std::string> paths = readRecentPaths();
    paths.erase(std::remove(paths.begin(), paths.end(), info.path), paths.end());
    paths.insert(paths.begin(), info.path);
    if (paths.size() > static_cast<size_t>(kMaxRecent)) paths.resize(kMaxRecent);
    writeRecentPaths(paths);
}
