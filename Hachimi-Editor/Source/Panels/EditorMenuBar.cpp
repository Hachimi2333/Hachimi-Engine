#include "Panels/EditorMenuBar.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "Utils/FileDialogs.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

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
            if (ImGui::MenuItem("Save Scene"))
            {
                SaveScene();
            }
            if (ImGui::MenuItem("Open Scene..."))
            {
                OpenScene();
            }
            if (ImGui::MenuItem("Import Texture..."))
            {
                ImportTexture();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Return To Project Hub"))
            {
                Application::Get().PopLayer(owner);
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

        if (context.ActiveScene != nullptr && ImGui::BeginMenu("Renderer"))
        {
            EnvironmentSettings& environment = context.ActiveScene->GetEnvironmentSettings();
            ImGui::Checkbox("Show Skybox", &environment.ShowSkybox);
            ImGui::SliderFloat("Exposure", &environment.Exposure, 0.1f, 4.0f);
            ImGui::SliderFloat("Environment Intensity", &environment.EnvironmentIntensity, 0.0f, 4.0f);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        std::string selectedScenePath;
        if (FileDialogs::DrawSceneFileDialog(selectedScenePath))
        {
            if (!selectedScenePath.empty())
            {
                const Ref<Project> project = ProjectManager::GetActiveProject();
                if (context.PlayState != EditorPlayState::Stopped)
                {
                    owner->OnStop();
                }

                if (project != nullptr && project->OpenScene(selectedScenePath))
                {
                    context.ActiveScene = project->GetActiveScene();
                    context.EditorScene = nullptr;
                    context.SelectedEntity = {};
                    context.PlayState = EditorPlayState::Stopped;
                    HE_CLIENT_INFO("Opened scene {}", selectedScenePath);
                }
            }
        }

        std::string selectedTexturePath;
        if (FileDialogs::DrawTextureImportDialog(selectedTexturePath))
        {
            if (!selectedTexturePath.empty())
            {
                AssetManager::ImportTexture(selectedTexturePath);
            }
        }
    }

    void EditorMenuBar::OpenScene()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            return;
        }

        FileDialogs::OpenSceneFileDialog(project->GetAssetsDirectory() / "Scenes");
    }

    void EditorMenuBar::SaveScene()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project != nullptr)
        {
            project->SaveActiveScene();
            HE_CLIENT_INFO("Scene saved");
        }
    }

    void EditorMenuBar::ImportTexture()
    {
        FileDialogs::OpenTextureImportDialog(AssetManager::GetAssetsDirectory());
    }
}
