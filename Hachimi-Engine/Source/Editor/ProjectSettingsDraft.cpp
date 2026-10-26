#include "Editor/ProjectSettingsDraft.h"

#include "Project/Project.h"
#include "Serialization/ProjectSerializer.h"
#include "Utils/FileSystem.h"

namespace HachimiEngine
{
    void ProjectSettingsDraft::Bind(const Project& project)
    {
        const auto* settings = project.GetBuildSettings().GetWindowsSettings();
        Value = settings != nullptr ? *settings : PlatformBuildSettings {};
        if (Value.ProductName.empty()) Value.ProductName = project.GetName();
    }

    bool ProjectSettingsDraft::Save(const Ref<Project>& project, std::string& error)
    {
        error.clear();
        if (project == nullptr) { error = "No active project"; return false; }
        if (Value.ProductName.find_first_not_of(" \t\r\n") == std::string::npos)
        { error = "Enter a product name"; return false; }
        if (Value.WindowWidth < 320 || Value.WindowHeight < 240 || Value.WindowWidth > 16384 || Value.WindowHeight > 16384)
        { error = "Window dimensions must be between 320 x 240 and 16384 x 16384"; return false; }
        const auto scene = Value.StartScene.lexically_normal();
        if (scene.empty() || scene.is_absolute() || *scene.begin() == ".."
            || scene.extension() != ".hscene" || !FileSystem::IsFile(project->GetProjectDirectory() / scene))
        { error = "Choose an existing start scene inside the project"; return false; }

        const auto before = project->GetBuildSettings();
        project->GetBuildSettings().GetOrCreateWindowsSettings() = Value;
        if (!ProjectSerializer(project).Serialize(project->GetProjectFilePath().string()))
        {
            project->GetBuildSettings() = before;
            error = "Could not save project settings; check the project file permissions";
            return false;
        }
        return true;
    }
}
