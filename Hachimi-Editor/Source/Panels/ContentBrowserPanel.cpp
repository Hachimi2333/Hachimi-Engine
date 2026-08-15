#include "Panels/ContentBrowserPanel.h"

#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "Utils/FileDialogs.h"
#include "Utils/FileSystem.h"

#include <imgui.h>

namespace HachimiEngine
{
    void ContentBrowserPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        ImGui::Begin("Content Browser");

        const std::filesystem::path assetsDirectory = AssetManager::GetAssetsDirectory();
        if (m_CurrentDirectory.empty() || assetsDirectory.empty())
        {
            m_CurrentDirectory = assetsDirectory;
        }

        if (ImGui::Button("Import Texture"))
        {
            FileDialogs::OpenTextureImportDialog(assetsDirectory);
        }

        std::string textureSourcePath;
        if (FileDialogs::DrawTextureImportDialog(textureSourcePath))
        {
            if (!textureSourcePath.empty())
            {
                AssetManager::ImportTexture(textureSourcePath);
            }
        }

        ImGui::Separator();

        if (m_CurrentDirectory != assetsDirectory)
        {
            if (ImGui::Button("<-"))
            {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
            }
        }

        ImGui::Text("%s", m_CurrentDirectory.string().c_str());
        ImGui::Separator();

        ImGui::Columns(2, "ContentBrowserColumns", false);
        ImGui::SetColumnWidth(0, 180.0f);
        ImGui::TextUnformatted("Name");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Type");
        ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& directory : FileSystem::GetDirectories(m_CurrentDirectory))
        {
            ImGui::Text("%s", FileSystem::GetFileName(directory).c_str());
            ImGui::NextColumn();
            ImGui::TextUnformatted("Folder");
            ImGui::NextColumn();

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_CurrentDirectory = directory;
            }
        }

        for (const auto& file : FileSystem::GetFiles(m_CurrentDirectory))
        {
            ImGui::Text("%s", FileSystem::GetFileName(file).c_str());
            ImGui::NextColumn();
            ImGui::Text("%s", FileSystem::GetExtension(file).c_str());
            ImGui::NextColumn();

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (FileSystem::GetExtension(file) == ".hscene")
                {
                    if (context.PlayState != EditorPlayState::Stopped)
                    {
                        owner->OnStop();
                    }

                    const Ref<Project> project = ProjectManager::GetActiveProject();
                    if (project != nullptr && project->OpenScene(file))
                    {
                        context.ActiveScene = project->GetActiveScene();
                        context.EditorScene = nullptr;
                        context.SelectedEntity = {};
                        context.PlayState = EditorPlayState::Stopped;
                        HE_CLIENT_INFO("Opened scene {}", file.string());
                    }
                }
            }
        }

        ImGui::Columns(1);
        ImGui::End();
    }
}
