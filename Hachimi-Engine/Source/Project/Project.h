#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Packaging/GameBuildSettings.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    class AssetDatabase;

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

        // The project does not own the asset database: the application does, because a renderer
        // pass needs it too. This is the pointer to the one instance that is currently loaded.
        void SetAssetDatabase(AssetDatabase* database) { m_AssetDatabase = database; }
        AssetDatabase* GetAssetDatabase() const { return m_AssetDatabase; }

        const std::filesystem::path& GetStartScenePath() const { return m_StartScenePath; }
        void SetStartScenePath(const std::filesystem::path& path) { m_StartScenePath = path; }

        // Scene the editor is currently working on. Saving writes here, not to the start scene:
        // writing the start scene unconditionally used to overwrite Default.hscene when another
        // scene had been opened.
        const std::filesystem::path& GetActiveScenePath() const { return m_ActiveScenePath; }

        const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
        void SetProjectFilePath(const std::filesystem::path& path) { m_ProjectFilePath = path; }

        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        void SetActiveScene(const Ref<Scene>& scene);

        GameBuildSettings& GetBuildSettings() { return m_BuildSettings; }
        const GameBuildSettings& GetBuildSettings() const { return m_BuildSettings; }

        bool OpenScene(const std::filesystem::path& scenePath);
        // Writes the active scene back to where it was opened from. Returns false when there is
        // no active scene or the file could not be written.
        bool SaveActiveScene();
        // Writes the active scene to a new path and makes that the active scene from then on.
        bool SaveActiveSceneAs(const std::filesystem::path& scenePath);

        static Ref<Project> CreateNew(const std::string& name, const std::filesystem::path& directory);

    private:
        std::string m_Name;
        std::filesystem::path m_ProjectDirectory;
        std::filesystem::path m_AssetsDirectory;
        std::filesystem::path m_StartScenePath;
        std::filesystem::path m_ActiveScenePath;
        std::filesystem::path m_ProjectFilePath;
        GameBuildSettings m_BuildSettings;
        Ref<Scene> m_ActiveScene;
        AssetDatabase* m_AssetDatabase = nullptr;
    };
}
