#include "Project/ProjectManager.h"

#include "Core/Log.h"
#include "Serialization/ProjectSerializer.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

namespace HachimiEngine
{
    Ref<Project> ProjectManager::s_ActiveProject;
    std::vector<std::filesystem::path> ProjectManager::s_RecentProjects;

    void ProjectManager::Init()
    {
        LoadRecentProjects();
    }

    Ref<Project> ProjectManager::CreateProject(const std::string& name, const std::filesystem::path& directory)
    {
        if (name.empty() || FileSystem::Exists(directory / name))
        {
            HE_CLIENT_ERROR("Project already exists or name is empty: {}", name);
            return nullptr;
        }

        const Ref<Project> project = Project::CreateNew(name, directory);
        if (project == nullptr)
        {
            return nullptr;
        }

        ProjectSerializer serializer(project);
        serializer.Serialize(project->GetProjectFilePath().string());

        s_ActiveProject = project;
        AddRecentProject(project->GetProjectFilePath());
        HE_CLIENT_INFO("Created project {} at {}", name, project->GetProjectDirectory().string());
        return project;
    }

    Ref<Project> ProjectManager::OpenProject(const std::filesystem::path& projectFilePath)
    {
        if (!FileSystem::Exists(projectFilePath))
        {
            HE_CLIENT_ERROR("Project file does not exist: {}", projectFilePath.string());
            return nullptr;
        }

        const Ref<Project> project = CreateRef<Project>();
        ProjectSerializer serializer(project);
        if (!serializer.Deserialize(projectFilePath.string()))
        {
            return nullptr;
        }

        // Older projects predate the scripting system; make the asset folder
        // available without touching any other project content.
        FileSystem::CreateDirectories(project->GetAssetsDirectory() / "Scripts");

        if (!FileSystem::Exists(project->GetStartScenePath()))
        {
            HE_CLIENT_WARN("Start scene is missing, creating a default scene");
            const Ref<Scene> defaultScene = CreateRef<Scene>();
            defaultScene->SetName("Default Scene");
            SceneSerializer sceneSerializer(defaultScene);
            sceneSerializer.Serialize(project->GetStartScenePath().string());
            project->SetActiveScene(defaultScene);
        }
        else
        {
            project->OpenScene(project->GetStartScenePath());
        }

        s_ActiveProject = project;
        AddRecentProject(projectFilePath);
        HE_CLIENT_INFO("Opened project {}", project->GetName());
        return project;
    }

    void ProjectManager::AddRecentProject(const std::filesystem::path& projectFilePath)
    {
        const std::filesystem::path normalized = projectFilePath.lexically_normal();

        std::erase(s_RecentProjects, normalized);
        s_RecentProjects.insert(s_RecentProjects.begin(), normalized);

        constexpr size_t MaxRecentProjects = 10;
        if (s_RecentProjects.size() > MaxRecentProjects)
        {
            s_RecentProjects.resize(MaxRecentProjects);
        }

        SaveRecentProjects();
    }

    void ProjectManager::RemoveRecentProject(const std::filesystem::path& projectFilePath)
    {
        std::erase(s_RecentProjects, projectFilePath.lexically_normal());
        SaveRecentProjects();
    }

    std::filesystem::path ProjectManager::GetHubSettingsPath()
    {
        return PlatformUtils::GetApplicationDataDirectory() / "HubSettings.yaml";
    }

    void ProjectManager::LoadRecentProjects()
    {
        s_RecentProjects.clear();

        const std::filesystem::path settingsPath = GetHubSettingsPath();
        if (!FileSystem::Exists(settingsPath))
        {
            return;
        }

        const YAML::Node data = YAML::LoadFile(settingsPath.string());
        const YAML::Node recentNode = data["RecentProjects"];
        if (!recentNode || !recentNode.IsSequence())
        {
            return;
        }

        for (const YAML::Node pathNode : recentNode)
        {
            const std::filesystem::path projectPath(pathNode.as<std::string>());
            if (FileSystem::Exists(projectPath))
            {
                s_RecentProjects.push_back(projectPath);
            }
        }
    }

    void ProjectManager::SaveRecentProjects()
    {
        const std::filesystem::path settingsPath = GetHubSettingsPath();
        FileSystem::CreateDirectories(settingsPath.parent_path());

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
        for (const auto& projectPath : s_RecentProjects)
        {
            out << projectPath.string();
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream file(settingsPath);
        file << out.c_str();
    }
}
