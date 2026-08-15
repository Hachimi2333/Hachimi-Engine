#include "Project/Project.h"

#include "Core/Log.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/FileSystem.h"

namespace HachimiEngine
{
    bool Project::OpenScene(const std::filesystem::path& scenePath)
    {
        if (!FileSystem::Exists(scenePath))
        {
            HE_CORE_ERROR("Scene file does not exist: {}", scenePath.string());
            return false;
        }

        const Ref<Scene> scene = CreateRef<Scene>();
        SceneSerializer serializer(scene);
        if (!serializer.Deserialize(scenePath.string()))
        {
            return false;
        }

        m_ActiveScene = scene;
        return true;
    }

    void Project::SaveActiveScene()
    {
        if (m_ActiveScene == nullptr)
        {
            HE_CORE_WARN("Cannot save scene: no active scene");
            return;
        }

        SceneSerializer serializer(m_ActiveScene);
        serializer.Serialize(m_StartScenePath.string());
    }

    Ref<Project> Project::CreateNew(const std::string& name, const std::filesystem::path& directory)
    {
        const std::filesystem::path projectDirectory = directory / name;

        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Meshes");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Textures");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Materials");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Scenes");

        const Ref<Scene> defaultScene = CreateRef<Scene>();
        defaultScene->SetName("Default Scene");

        const std::filesystem::path startScenePath = projectDirectory / "Assets" / "Scenes" / "Default.hscene";
        SceneSerializer serializer(defaultScene);
        serializer.Serialize(startScenePath.string());

        const Ref<Project> project = CreateRef<Project>();
        project->m_Name = name;
        project->m_ProjectDirectory = projectDirectory;
        project->m_AssetsDirectory = projectDirectory / "Assets";
        project->m_StartScenePath = startScenePath;
        project->m_ProjectFilePath = projectDirectory / (name + ".hproj");
        project->m_ActiveScene = defaultScene;
        return project;
    }
}
