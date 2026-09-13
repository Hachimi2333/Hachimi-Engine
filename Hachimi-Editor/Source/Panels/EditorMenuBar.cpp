#include "Panels/EditorMenuBar.h"

#include "Asset/AssetDatabase.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "Utils/FileDialogs.h"

#include <imgui.h>

#include <filesystem>

namespace HachimiEngine
{
    void EditorMenuBar::Draw(EditorLayer* owner, EditorContext& context)
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                owner->RequestAction("creating a new scene", [owner] { owner->CreateNewScene(); });
            }
            if (ImGui::MenuItem("New Material"))
            {
                owner->CreateNewMaterial();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            {
                owner->SaveActiveScene();
            }
            if (ImGui::MenuItem("Save Scene As..."))
            {
                owner->SaveSceneAsWithDialog();
            }
            if (ImGui::MenuItem("Open Scene..."))
            {
                owner->OpenSceneWithDialog();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Import Asset..."))
            {
                ImportAsset(*owner, context);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Return To Project Hub"))
            {
                owner->RequestAction("returning to the project hub", [owner]
                {
                    Application::Get().PopLayer(owner);
                });
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            const bool canUndo = context.History != nullptr && context.History->CanUndo();
            const bool canRedo = context.History != nullptr && context.History->CanRedo();

            const std::string undoLabel = canUndo ? "Undo " + context.History->GetUndoName() : "Undo";
            const std::string redoLabel = canRedo ? "Redo " + context.History->GetRedoName() : "Redo";

            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo))
            {
                context.History->Undo();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo))
            {
                context.History->Redo();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Reset Layout"))
            {
                owner->ResetLayout();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build"))
        {
            if (ImGui::MenuItem("Build Settings..."))
            {
                owner->OpenBuildSettings();
            }

            ImGui::EndMenu();
        }

        if (context.ActiveScene != nullptr && ImGui::BeginMenu("Renderer"))
        {
            EnvironmentSettings& environment = context.ActiveScene->GetEnvironmentSettings();
            ImGui::Checkbox("Show Skybox", &environment.ShowSkybox);
            ImGui::SliderFloat("Exposure", &environment.Exposure, 0.1f, 4.0f);
            ImGui::SliderFloat("Environment Intensity", &environment.EnvironmentIntensity, 0.0f, 4.0f);
            ImGui::EndMenu();
        }

        // The scene name doubles as the unsaved-changes indicator, which is why it is drawn from
        // the live dirty state rather than from a cached string.
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project != nullptr)
        {
            const bool dirty = context.DirtyState != nullptr && context.DirtyState->IsDirty();
            const std::string title = project->GetName()
                + (dirty ? " *" : "")
                + "  -  " + project->GetActiveScenePath().filename().string();

            const float textWidth = ImGui::CalcTextSize(title.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - ImGui::GetStyle().ItemSpacing.x * 3.0f);
            ImGui::TextDisabled("%s", title.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    void EditorMenuBar::ImportAsset(EditorLayer& layer, EditorContext& context)
    {
        (void)layer;

        if (context.Assets == nullptr)
        {
            return;
        }

        const std::filesystem::path selectedPath = FileDialogs::OpenAssetImportDialog(
            context.Assets->GetAssetsDirectory());
        if (selectedPath.empty())
        {
            return;
        }

        // Importing into the content root keeps the menu entry predictable; the content browser's
        // own toolbar imports into the folder the user is browsing.
        AssetHandle imported;
        const AssetWriteResult result = context.Assets->ImportAsset(
            selectedPath,
            context.Assets->GetAssetsDirectory(),
            imported);

        if (result != AssetWriteResult::Success)
        {
            HE_CLIENT_ERROR("Import failed: {}", ToString(result));
            return;
        }

        context.SelectAsset(imported, context.Assets->GetAssetPath(imported));
        HE_CLIENT_INFO("Imported {}", context.SelectedAssetPath.string());
    }
}
