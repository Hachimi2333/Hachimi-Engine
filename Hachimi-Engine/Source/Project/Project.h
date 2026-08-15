#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // Describes one Hachimi project directory layout and its active scene.
    class Project
    {
    public:
        Project() = default;

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
        void SetProjectDirectory(const std::filesystem::path& directory) { m_ProjectDirectory = directory; }

        const std::filesystem::path& GetAssetsDirectory() const { return m_AssetsDirectory; }
        void SetAssetsDirectory(const std::filesystem::path& directory) { m_AssetsDirectory = directory; }

        const std::filesystem::path& GetStartScenePath() const { return m_StartScenePath; }
        void SetStartScenePath(const std::filesystem::path& path) { m_StartScenePath = path; }

        const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
        void SetProjectFilePath(const std::filesystem::path& path) { m_ProjectFilePath = path; }

        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        void SetActiveScene(const Ref<Scene>& scene) { m_ActiveScene = scene; }

        bool OpenScene(const std::filesystem::path& scenePath);
        void SaveActiveScene();

        static Ref<Project> CreateNew(const std::string& name, const std::filesystem::path& directory);

    private:
        std::string m_Name;
        std::filesystem::path m_ProjectDirectory;
        std::filesystem::path m_AssetsDirectory;
        std::filesystem::path m_StartScenePath;
        std::filesystem::path m_ProjectFilePath;
        Ref<Scene> m_ActiveScene;
    };
}
