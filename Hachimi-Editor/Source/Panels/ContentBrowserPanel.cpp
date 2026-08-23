#include "Panels/ContentBrowserPanel.h"

#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "UI/AssetBrowserGrid.h"
#include "Utils/FileDialogs.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* BackGlyph = "\uE72B";
        constexpr const char* ForwardGlyph = "\uE72A";
        constexpr const char* UpGlyph = "\uE74A";

        bool DrawToolbarButton(const char* glyph, const char* tooltip, bool enabled)
        {
            ImGui::BeginDisabled(!enabled);
            const bool clicked = ImGui::Button(glyph, ImVec2(ImGui::GetFrameHeight() * 1.6f, 0.0f));
            ImGui::EndDisabled();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", tooltip);
            }

            return clicked;
        }
    }

    void ContentBrowserPanel::NavigateTo(const std::filesystem::path& directory)
    {
        if (m_CurrentDirectory == directory)
        {
            return;
        }

        if (!m_CurrentDirectory.empty())
        {
            m_BackHistory.push_back(m_CurrentDirectory);
        }
        m_ForwardHistory.clear();
        m_CurrentDirectory = directory;
        m_SelectedPath.clear();
    }

    void ContentBrowserPanel::NavigateBack()
    {
        if (m_BackHistory.empty())
        {
            return;
        }

        m_ForwardHistory.push_back(m_CurrentDirectory);
        m_CurrentDirectory = m_BackHistory.back();
        m_BackHistory.pop_back();
        m_SelectedPath.clear();
    }

    void ContentBrowserPanel::NavigateForward()
    {
        if (m_ForwardHistory.empty())
        {
            return;
        }

        m_BackHistory.push_back(m_CurrentDirectory);
        m_CurrentDirectory = m_ForwardHistory.back();
        m_ForwardHistory.pop_back();
        m_SelectedPath.clear();
    }

    void ContentBrowserPanel::NavigateUp(const std::filesystem::path& assetsDirectory)
    {
        if (m_CurrentDirectory == assetsDirectory)
        {
            return;
        }

        NavigateTo(m_CurrentDirectory.parent_path());
    }

    void ContentBrowserPanel::DrawBreadcrumb(const std::filesystem::path& assetsDirectory)
    {
        if (m_CurrentDirectory == assetsDirectory)
        {
            ImGui::TextUnformatted("Assets");
            return;
        }

        ImGui::TextUnformatted("Assets");

        const std::filesystem::path relativePath = m_CurrentDirectory.lexically_relative(assetsDirectory);
        std::filesystem::path accumulatedPath = assetsDirectory;
        const size_t componentCount = std::distance(relativePath.begin(), relativePath.end());

        size_t componentIndex = 0;
        for (const auto& component : relativePath)
        {
            accumulatedPath /= component;

            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();

            if (componentIndex + 1 < componentCount)
            {
                if (ImGui::Button(component.string().c_str()))
                {
                    NavigateTo(accumulatedPath);
                }
            }
            else
            {
                ImGui::TextUnformatted(component.string().c_str());
            }

            ++componentIndex;
        }
    }

    void ContentBrowserPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        ImGui::Begin("Content Browser");

        const std::filesystem::path assetsDirectory = AssetManager::GetAssetsDirectory();
        if (m_CurrentDirectory.empty() || assetsDirectory.empty())
        {
            m_CurrentDirectory = assetsDirectory;
        }

        if (!FileSystem::IsDirectory(m_CurrentDirectory))
        {
            m_CurrentDirectory = assetsDirectory;
            m_BackHistory.clear();
            m_ForwardHistory.clear();
            m_SelectedPath.clear();
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const float importButtonWidth = ImGui::CalcTextSize("Import Texture").x + style.FramePadding.x * 2.0f;

        if (DrawToolbarButton(BackGlyph, "Back", !m_BackHistory.empty()))
        {
            NavigateBack();
        }
        ImGui::SameLine();
        if (DrawToolbarButton(ForwardGlyph, "Forward", !m_ForwardHistory.empty()))
        {
            NavigateForward();
        }
        ImGui::SameLine();
        if (DrawToolbarButton(UpGlyph, "Up", m_CurrentDirectory != assetsDirectory))
        {
            NavigateUp(assetsDirectory);
        }

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - importButtonWidth - style.WindowPadding.x);
        if (ImGui::Button("Import Texture", ImVec2(importButtonWidth, 0.0f)))
        {
            const std::filesystem::path selectedTexturePath =
                FileDialogs::OpenTextureImportDialog(assetsDirectory);
            if (!selectedTexturePath.empty())
            {
                AssetManager::ImportTexture(selectedTexturePath);
            }
        }

        DrawBreadcrumb(assetsDirectory);

        ImGui::Separator();

        const std::vector<std::filesystem::path> directories = FileSystem::GetDirectories(m_CurrentDirectory);
        const std::vector<std::filesystem::path> files = FileSystem::GetFiles(m_CurrentDirectory);
        std::filesystem::path activatedPath;
        AssetBrowserGrid::Draw(directories, files, m_SelectedPath, activatedPath);

        if (activatedPath.empty())
        {
            ImGui::End();
            return;
        }

        if (FileSystem::IsDirectory(activatedPath))
        {
            NavigateTo(activatedPath);
            ImGui::End();
            return;
        }

        const std::string extension = FileSystem::GetExtension(activatedPath);
        if (extension == ".hscene")
        {
            if (context.PlayState != EditorPlayState::Stopped)
            {
                owner->OnStop();
            }

            const Ref<Project> project = ProjectManager::GetActiveProject();
            if (project != nullptr && project->OpenScene(activatedPath))
            {
                context.ActiveScene = project->GetActiveScene();
                context.EditorScene = nullptr;
                context.SelectedEntity = {};
                context.PlayState = EditorPlayState::Stopped;
                HE_CLIENT_INFO("Opened scene {}", activatedPath.string());
            }
        }
        else
        {
            // Scripts and unknown asset files are handed to the system editor.
            PlatformUtils::OpenPathInExplorer(activatedPath);
        }

        ImGui::End();
    }
}
