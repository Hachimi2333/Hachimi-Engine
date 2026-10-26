#include <doctest/doctest.h>

#include "Editor/CommandHistory.h"
#include "Editor/ProjectSettingsDraft.h"
#include "Editor/SceneEnvironmentCommands.h"
#include "Editor/TextureSettingsDraft.h"
#include "Project/Project.h"
#include "Scene/Scene.h"
#include "Serialization/ProjectSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE("Editor")
{
    TEST_CASE("environment settings mark the scene dirty and support undo and redo")
    {
        Scene scene;
        CommandHistory history(scene);
        history.ExecuteMerged(SceneEnvironmentCommands::MakeSetExposure(1.0f, 2.0f));
        history.ExecuteMerged(SceneEnvironmentCommands::MakeSetExposure(2.0f, 3.0f));
        CHECK(scene.IsDirty());
        CHECK(history.GetUndoCount() == 1);
        REQUIRE(history.Undo());
        CHECK(scene.GetEnvironmentSettings().Exposure == 1.0f);
        REQUIRE(history.Redo());
        CHECK(scene.GetEnvironmentSettings().Exposure == 3.0f);
        history.Execute(SceneEnvironmentCommands::MakeSetSkybox(true, false));
        REQUIRE(history.Undo());
        CHECK(scene.GetEnvironmentSettings().ShowSkybox);
        history.Execute(SceneEnvironmentCommands::MakeSetIntensity(1.0f, 0.5f));
        REQUIRE(history.Undo());
        CHECK(scene.GetEnvironmentSettings().EnvironmentIntensity == 1.0f);
    }

    TEST_CASE("texture settings survive subsequent frames and revert from the asset record")
    {
        const auto meta = AssetMeta::MakeDefault(AssetType::Texture);
        const AssetHandle handle { meta.ID, AssetType::Texture };
        TextureSettingsDraft draft;
        draft.Bind(handle, meta);
        draft.Value.Type = TextureType::Normal;
        draft.Value.GenerateMipmaps = false;
        draft.Changed = true;
        for (int frame = 0; frame < 5; ++frame) draft.Bind(handle, meta);
        CHECK(draft.Changed);
        CHECK(draft.Value.Type == TextureType::Normal);
        CHECK_FALSE(draft.Value.GenerateMipmaps);
        CHECK(meta.Texture.Type == TextureType::Color);
        draft.Revert(handle, meta);
        CHECK_FALSE(draft.Changed);
        CHECK(draft.Value.Type == TextureType::Color);
    }

    TEST_CASE("project setting drafts commit only on successful saves")
    {
        const auto directory = Workspace().PrepareDirectory("project_settings_draft");
        auto project = CreateRef<Project>();
        project->SetName("Settings Test");
        project->SetProjectDirectory(directory);
        project->SetAssetsDirectory(directory / "Assets");
        project->SetStartScenePath(directory / "Assets/Scenes/Default.hscene");
        project->SetProjectFilePath(directory / "Test.hproj");
        auto& live = project->GetBuildSettings().GetOrCreateWindowsSettings();
        live.ProductName = "Original";
        live.StartScene = "Assets/Scenes/Default.hscene";
        REQUIRE(FileSystem::WriteTextFile(directory / live.StartScene, "Scene: Default\n"));
        ProjectSettingsDraft draft;
        draft.Bind(*project);
        draft.Value.ProductName = "Edited";
        CHECK(live.ProductName == "Original");
        std::string error;
        REQUIRE(draft.Save(project, error));
        CHECK(project->GetBuildSettings().GetWindowsSettings()->ProductName == "Edited");
        auto reloaded = CreateRef<Project>();
        REQUIRE(ProjectSerializer(reloaded).Deserialize(project->GetProjectFilePath().string()));
        CHECK(reloaded->GetBuildSettings().GetWindowsSettings()->ProductName == "Edited");

        draft.Value.ProductName = "Unwritten";
        project->SetProjectFilePath(directory); // A directory cannot be replaced with a settings file.
        CHECK_FALSE(draft.Save(project, error));
        CHECK_FALSE(error.empty());
        CHECK(project->GetBuildSettings().GetWindowsSettings()->ProductName == "Edited");
        draft.Value.StartScene = "../Outside.hscene";
        CHECK_FALSE(draft.Save(project, error));
    }
}
