#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Project/Project.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    // Owns the active project and the recent-projects registry.
    class ProjectManager
    {
    public:
        static void Init();

        static Ref<Project> CreateProject(const std::string& name, const std::filesystem::path& directory);
        static Ref<Project> OpenProject(const std::filesystem::path& projectFilePath);

        static Ref<Project> GetActiveProject() { return s_ActiveProject; }
        static void SetActiveProject(const Ref<Project>& project) { s_ActiveProject = project; }

        static const std::vector<std::filesystem::path>& GetRecentProjects() { return s_RecentProjects; }
        static void AddRecentProject(const std::filesystem::path& projectFilePath);
        static void RemoveRecentProject(const std::filesystem::path& projectFilePath);

        static std::filesystem::path GetHubSettingsPath();

    private:
        static void LoadRecentProjects();
        static void SaveRecentProjects();

    private:
        static Ref<Project> s_ActiveProject;
        static std::vector<std::filesystem::path> s_RecentProjects;
    };
}
