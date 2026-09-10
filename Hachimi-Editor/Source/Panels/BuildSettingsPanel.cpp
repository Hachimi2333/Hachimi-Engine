#include "Panels/BuildSettingsPanel.h"

#include "Core/Log.h"
#include "Packaging/ProjectPackager.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/Project.h"
#include "Project/ProjectManager.h"
#include "Serialization/ProjectSerializer.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        struct SceneOption
        {
            std::filesystem::path ProjectRelativePath;
            std::string DisplayName;
        };

        std::vector<SceneOption> GetSceneOptions(const Ref<Project>& project)
        {
            std::vector<SceneOption> options;
            const std::filesystem::path scenesDirectory = project->GetAssetsDirectory() / "Scenes";

            for (const std::filesystem::path& scenePath : FileSystem::GetFiles(scenesDirectory))
            {
                if (FileSystem::GetExtension(scenePath) != ".hscene")
                {
                    continue;
                }

                std::error_code errorCode;
                const std::filesystem::path relativePath =
                    std::filesystem::relative(scenePath, project->GetProjectDirectory(), errorCode);
                if (errorCode)
                {
                    continue;
                }

                SceneOption& option = options.emplace_back();
                option.ProjectRelativePath = relativePath.lexically_normal();
                option.DisplayName = relativePath.generic_string();
            }
            return options;
        }

        int GetSelectedSceneIndex(
            const std::vector<SceneOption>& options,
            const std::filesystem::path& selectedScene)
        {
            const std::string selectedName = selectedScene.lexically_normal().generic_string();
            for (size_t i = 0; i < options.size(); ++i)
            {
                if (options[i].ProjectRelativePath.generic_string() == selectedName)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }
    }

    void BuildSettingsPanel::Open()
    {
        m_Open = true;
        m_StatusMessage.clear();
        m_StatusIsError = false;

        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project != nullptr)
        {
            if (const PlatformBuildSettings* settings = project->GetBuildSettings().GetWindowsSettings())
            {
                std::snprintf(m_ProductName, sizeof(m_ProductName), "%s", settings->ProductName.c_str());
            }
        }
    }

    void BuildSettingsPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        if (!m_Open)
        {
            return;
        }

        ImGui::OpenPopup("Build Settings##BuildSettingsPopup");
        if (!ImGui::BeginPopupModal("Build Settings##BuildSettingsPopup", &m_Open, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            ImGui::TextUnformatted("No active project");
            ImGui::EndPopup();
            return;
        }

        GameBuildSettings& buildSettings = project->GetBuildSettings();
        PlatformBuildSettings& windowsSettings = buildSettings.GetOrCreateWindowsSettings();
        if (m_ProductName[0] == '\0')
        {
            std::snprintf(m_ProductName, sizeof(m_ProductName), "%s", windowsSettings.ProductName.c_str());
        }

        if (ImGui::BeginTabBar("##BuildPlatformTabs"))
        {
            if (ImGui::TabItemButton("Windows", ImGuiTabItemFlags_SetSelected))
            {
            }

            ImGui::BeginDisabled();
            ImGui::TabItemButton("macOS");
            ImGui::TabItemButton("Linux");
            ImGui::EndDisabled();

            ImGui::EndTabBar();
        }

        ImGui::TextUnformatted("Target Platform: Windows x86_64");

        ImGui::Spacing();

        ImGui::InputText("Product Name", m_ProductName, sizeof(m_ProductName));

        const std::vector<SceneOption> sceneOptions = GetSceneOptions(project);
        if (!sceneOptions.empty() && windowsSettings.StartScene.empty())
        {
            windowsSettings.StartScene = sceneOptions.front().ProjectRelativePath;
        }

        const int selectedSceneIndex = GetSelectedSceneIndex(sceneOptions, windowsSettings.StartScene);
        if (ImGui::BeginCombo("Start Scene", selectedSceneIndex >= 0 ? sceneOptions[selectedSceneIndex].DisplayName.c_str() : windowsSettings.StartScene.generic_string().c_str()))
        {
            for (size_t i = 0; i < sceneOptions.size(); ++i)
            {
                const bool isSelected = static_cast<int>(i) == selectedSceneIndex;
                if (ImGui::Selectable(sceneOptions[i].DisplayName.c_str(), isSelected))
                {
                    windowsSettings.StartScene = sceneOptions[i].ProjectRelativePath;
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        int windowWidth = static_cast<int>(windowsSettings.WindowWidth);
        if (ImGui::InputInt("Window Width", &windowWidth, 16))
        {
            windowsSettings.WindowWidth = static_cast<uint32_t>(std::max(windowWidth, 320));
        }

        int windowHeight = static_cast<int>(windowsSettings.WindowHeight);
        if (ImGui::InputInt("Window Height", &windowHeight, 16))
        {
            windowsSettings.WindowHeight = static_cast<uint32_t>(std::max(windowHeight, 240));
        }

        ImGui::Checkbox("VSync", &windowsSettings.VSync);

        ImGui::Spacing();
        const std::filesystem::path outputDirectory = ProjectPackager::GetWindowsOutputDirectory(*project);
        ImGui::TextUnformatted("Output:");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", outputDirectory.string().c_str());

        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120.0f, 0.0f)))
        {
            if (context.PlayState != EditorPlayState::Stopped)
            {
                owner->OnStop();
            }

            if (project->GetActiveScene() != nullptr)
            {
                project->SaveActiveScene();
            }

            windowsSettings.ProductName = m_ProductName;
            ProjectSerializer projectSerializer(project);
            projectSerializer.Serialize(project->GetProjectFilePath().string());

            const std::filesystem::path playerExecutablePath =
                PlatformUtils::GetExecutableDirectory() / "Hachimi-Player.exe";
            const GameExportResult result = ProjectPackager::Export(project, playerExecutablePath);

            m_StatusMessage = result.Message;
            m_StatusIsError = !result.Success;
            m_LastOutputDirectory = result.OutputDirectory;

            if (result.Success)
            {
                m_HasReport = true;
                m_Report = result.Report;
                HE_CLIENT_INFO("{}", result.Message);
            }
            else
            {
                m_HasReport = false;
                HE_CLIENT_ERROR("{}", result.Message);
            }
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_LastOutputDirectory.empty() || !FileSystem::Exists(m_LastOutputDirectory));
        if (ImGui::Button("Open Folder", ImVec2(120.0f, 0.0f)))
        {
            PlatformUtils::OpenPathInExplorer(m_LastOutputDirectory);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80.0f, 0.0f)))
        {
            m_Open = false;
        }

        if (!m_StatusMessage.empty())
        {
            ImGui::TextWrapped("%s%s", m_StatusIsError ? "Error: " : "", m_StatusMessage.c_str());
        }

        if (m_HasReport)
        {
            ImGui::Spacing();
            ImGui::SeparatorText("Package report");
            ImGui::Text("Entries: %u  (%u stored, %u zstd)",
                m_Report.EntryCount, m_Report.StoredEntryCount, m_Report.ZstdEntryCount);
            ImGui::Text("Blocks: %u", m_Report.BlockCount);
            ImGui::Text("Raw: %.2f MiB", static_cast<double>(m_Report.UncompressedBytes) / (1024.0 * 1024.0));
            ImGui::Text("Packed: %.2f MiB", static_cast<double>(m_Report.CompressedBytes) / (1024.0 * 1024.0));
            ImGui::Text("Ratio: %.1f%%", m_Report.CompressionRatio() * 100.0);
            ImGui::Text("Dictionary: %u bytes", m_Report.DictionarySize);
            ImGui::Text("Took: %.2f s", m_Report.Seconds);
            ImGui::TextDisabled("Package id: 0x%016llX", static_cast<unsigned long long>(m_Report.PackageId));
        }

        ImGui::EndPopup();
    }
}
