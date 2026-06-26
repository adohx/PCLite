#ifndef PCLITE_PROJECT_MANAGER_H
#define PCLITE_PROJECT_MANAGER_H

#include <filesystem>
#include <string>
#include <vector>

#include "app_paths.h"

// A project is a directory under `projectsDir` containing a `project.json`
// (this struct's fields) plus the converted PCLite dataset
// (octree.bin/hierarchy.bin/metadata.json/kdtree.bin/kdtree_index.bin) --
// i.e. `path` is exactly the directory a PointCloudLoader would be pointed at.
struct ProjectInfo {
    std::string name;       // user-facing display name
    std::string path;       // directory containing project.json + the dataset
    std::string sourceFile; // original LAS path, kept for display only
    std::string createdAt;  // display string, e.g. "2026-06-26 18:30"
};

// Pure filesystem + JSON (no GL/viewer dependency), so it's unit-testable
// headlessly. The real app uses the default (app_paths::projectsDir()/
// recentListPath()); tests point it at a temp directory instead.
class ProjectManager {
public:
    explicit ProjectManager(std::filesystem::path projectsDir = app_paths::projectsDir(),
                             std::filesystem::path recentListPath = app_paths::recentListPath());

    // All projects under projectsDir with a valid project.json, newest first.
    std::vector<ProjectInfo> listProjects() const;

    // The persisted recent list (most-recently-opened first), filtered to
    // entries that still exist; self-heals the persisted list if any don't.
    std::vector<ProjectInfo> recentProjects() const;

    // Creates a new project directory (sanitized, deduplicated name under
    // projectsDir) and writes project.json. Does not run any conversion --
    // that's the caller's job, so import progress can be reported
    // separately from project bookkeeping.
    ProjectInfo createProject(const std::string& name, const std::string& sourceLasPath) const;

    // Removes the project's entire directory and drops it from the recent
    // list. Returns true if the directory no longer exists afterward.
    bool deleteProject(const ProjectInfo& info) const;

    // Moves (or inserts) `info` to the front of the recent list, persists,
    // and caps the list at kMaxRecent entries.
    void touchRecent(const ProjectInfo& info) const;

    static constexpr int kMaxRecent = 10;

private:
    std::filesystem::path projectsDir_;
    std::filesystem::path recentListPath_;

    std::vector<std::string> readRecentPaths() const;
    void writeRecentPaths(const std::vector<std::string>& paths) const;
};

// Reads `dir/project.json`; returns false (info left unchanged) if missing
// or unparseable.
bool loadProjectInfo(const std::filesystem::path& dir, ProjectInfo& info);

#endif //PCLITE_PROJECT_MANAGER_H
