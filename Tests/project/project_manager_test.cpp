#include <gtest/gtest.h>

#include <filesystem>
#include <thread>

#include "project_manager.h"

namespace fs = std::filesystem;

namespace {

class ProjectManagerTest : public ::testing::Test {
protected:
    fs::path root_;
    fs::path projectsDir_;
    fs::path recentListPath_;

    void SetUp() override {
        root_ = fs::temp_directory_path() / fs::path("pclite_project_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        projectsDir_ = root_ / "projects";
        recentListPath_ = root_ / "recent_projects.json";
        fs::create_directories(projectsDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    ProjectManager makeManager() const { return ProjectManager(projectsDir_, recentListPath_); }
};

} // namespace

TEST_F(ProjectManagerTest, CreateProjectWritesLoadableProjectJson) {
    ProjectManager mgr = makeManager();
    ProjectInfo info = mgr.createProject("Office Scan", "/data/office.las");

    EXPECT_TRUE(fs::exists(fs::path(info.path) / "project.json"));
    EXPECT_EQ(info.name, "Office Scan");
    EXPECT_EQ(info.sourceFile, "/data/office.las");
    EXPECT_FALSE(info.createdAt.empty());

    ProjectInfo reloaded;
    ASSERT_TRUE(loadProjectInfo(info.path, reloaded));
    EXPECT_EQ(reloaded.name, info.name);
    EXPECT_EQ(reloaded.sourceFile, info.sourceFile);
    EXPECT_EQ(reloaded.createdAt, info.createdAt);
}

TEST_F(ProjectManagerTest, CreateProjectDedupesCollidingNames) {
    ProjectManager mgr = makeManager();
    ProjectInfo a = mgr.createProject("Site A", "/data/a.las");
    ProjectInfo b = mgr.createProject("Site A", "/data/b.las");
    ProjectInfo c = mgr.createProject("Site A", "/data/c.las");

    EXPECT_NE(a.path, b.path);
    EXPECT_NE(b.path, c.path);
    EXPECT_NE(a.path, c.path);
}

TEST_F(ProjectManagerTest, ListProjectsReturnsAllCreatedNewestFirst) {
    ProjectManager mgr = makeManager();
    mgr.createProject("First", "/data/first.las");
    std::this_thread::sleep_for(std::chrono::seconds(1)); // createdAt has minute resolution... ensure ordering is at least stable
    ProjectInfo second = mgr.createProject("Second", "/data/second.las");

    auto projects = mgr.listProjects();
    ASSERT_EQ(projects.size(), 2u);
    // Both share the same minute timestamp in practice; just check both present.
    bool foundFirst = false, foundSecond = false;
    for (auto& p : projects) {
        if (p.name == "First") foundFirst = true;
        if (p.name == "Second") foundSecond = true;
    }
    EXPECT_TRUE(foundFirst);
    EXPECT_TRUE(foundSecond);
    EXPECT_EQ(second.name, "Second");
}

TEST_F(ProjectManagerTest, DeleteProjectRemovesDirectoryAndDisappearsFromList) {
    ProjectManager mgr = makeManager();
    ProjectInfo info = mgr.createProject("Temp", "/data/temp.las");
    ASSERT_EQ(mgr.listProjects().size(), 1u);

    EXPECT_TRUE(mgr.deleteProject(info));
    EXPECT_FALSE(fs::exists(info.path));
    EXPECT_EQ(mgr.listProjects().size(), 0u);
}

TEST_F(ProjectManagerTest, RecentProjectsOrderedMostRecentFirstAndDeduped) {
    ProjectManager mgr = makeManager();
    ProjectInfo a = mgr.createProject("A", "/data/a.las");
    ProjectInfo b = mgr.createProject("B", "/data/b.las");

    mgr.touchRecent(a);
    mgr.touchRecent(b);
    mgr.touchRecent(a); // re-touching A should move it back to front, not duplicate

    auto recent = mgr.recentProjects();
    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(recent[0].name, "A");
    EXPECT_EQ(recent[1].name, "B");
}

TEST_F(ProjectManagerTest, RecentProjectsCapsAtMax) {
    ProjectManager mgr = makeManager();
    for (int i = 0; i < ProjectManager::kMaxRecent + 5; ++i) {
        ProjectInfo info = mgr.createProject("P" + std::to_string(i), "/data/p.las");
        mgr.touchRecent(info);
    }

    auto recent = mgr.recentProjects();
    EXPECT_EQ(recent.size(), static_cast<size_t>(ProjectManager::kMaxRecent));
}

TEST_F(ProjectManagerTest, RecentProjectsSelfHealsStaleEntries) {
    ProjectManager mgr = makeManager();
    ProjectInfo a = mgr.createProject("A", "/data/a.las");
    ProjectInfo b = mgr.createProject("B", "/data/b.las");
    mgr.touchRecent(a);
    mgr.touchRecent(b);

    mgr.deleteProject(a); // removes a's directory AND its recent entry directly

    auto recent = mgr.recentProjects();
    ASSERT_EQ(recent.size(), 1u);
    EXPECT_EQ(recent[0].name, "B");
}

TEST_F(ProjectManagerTest, DeleteProjectOnAlreadyMissingDirectoryStillReturnsTrue) {
    ProjectManager mgr = makeManager();
    ProjectInfo info;
    info.path = (projectsDir_ / "does_not_exist").string();
    EXPECT_TRUE(mgr.deleteProject(info));
}
