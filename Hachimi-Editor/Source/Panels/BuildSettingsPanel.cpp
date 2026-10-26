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
        std::vector<std::filesystem::path> GetSceneOptions(const Ref<Project>& project)
        {
            std::vector<std::filesystem::path> options;

            for (const std::filesystem::path& scenePath : FileSystem::GetFilesRecursive(project->GetAssetsDirectory()))
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

                options.push_back(relativePath.lexically_normal());
            }
            return options;
        }

        int GetSelectedSceneIndex(
            const std::vector<std::filesystem::path>& options,
            const std::filesystem::path& selectedScene)
        {
            const std::string selectedName = selectedScene.lexically_normal().generic_string();
            for (size_t i = 0; i < options.size(); ++i)
            {
                if (options[i].generic_string() == selectedName)
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
        m_HasReport = false;

        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project != nullptr)
        {
            m_Draft.Bind(*project);
            m_SceneOptions = GetSceneOptions(project);
            std::snprintf(m_ProductName, sizeof(m_ProductName), "%s", m_Draft.Value.ProductName.c_str());
        }
    }

    void BuildSettingsPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        if (!m_Open)
        {
            return;
        }

        ImGui::OpenPopup("Project Settings##ProjectSettingsPopup");
        if (!ImGui::BeginPopupModal("Project Settings##ProjectSettingsPopup", &m_Open, ImGuiWindowFlags_AlwaysAutoResize))
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

        PlatformBuildSettings& windowsSettings = m_Draft.Value;
        ImGui::TextDisabled("Windows x86_64");
        ImGui::SeparatorText("Game");
        ImGui::InputText("Product Name", m_ProductName, sizeof(m_ProductName));

        const auto& sceneOptions = m_SceneOptions;
        if (!sceneOptions.empty() && windowsSettings.StartScene.empty())
        {
            windowsSettings.StartScene = sceneOptions.front();
        }

        const int selectedSceneIndex = GetSelectedSceneIndex(sceneOptions, windowsSettings.StartScene);
        if (ImGui::BeginCombo("Start Scene", selectedSceneIndex >= 0 ? sceneOptions[selectedSceneIndex].generic_string().c_str() : windowsSettings.StartScene.generic_string().c_str()))
        {
            for (size_t i = 0; i < sceneOptions.size(); ++i)
            {
                const bool isSelected = static_cast<int>(i) == selectedSceneIndex;
                if (ImGui::Selectable(sceneOptions[i].generic_string().c_str(), isSelected))
                {
                    windowsSettings.StartScene = sceneOptions[i];
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText("Window");
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

        ImGui::SeparatorText("Export");
        const std::filesystem::path outputDirectory = ProjectPackager::GetWindowsOutputDirectory(*project);
        ImGui::TextUnformatted("Output:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", outputDirectory.string().c_str());

        ImGui::Spacing();

        if (ImGui::Button("Save Settings"))
        {
            windowsSettings.ProductName = m_ProductName;
            m_StatusIsError = !m_Draft.Save(project, m_StatusMessage);
            if (!m_StatusIsError) m_StatusMessage = "Project settings saved";
        }
        ImGui::SameLine();
        const bool canExport = context.PlayState == EditorPlayState::Stopped && !owner->HasUnappliedAssetChanges();
        ImGui::BeginDisabled(!canExport);
        const bool exportRequested = ImGui::Button("Save and Export");
        ImGui::EndDisabled();
        if (!canExport && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Stop playback and apply or revert pending asset edits before exporting");
        if (exportRequested)
        {
            windowsSettings.ProductName = m_ProductName;
            m_StatusIsError = !m_Draft.Save(project, m_StatusMessage);
            if (!m_StatusIsError && project->GetActiveScene() != nullptr && !owner->SaveActiveScene())
            {
                m_StatusMessage = "Cannot export: scene save failed";
                m_StatusIsError = true;
            }
            if (!m_StatusIsError)
            {
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
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_LastOutputDirectory.empty() || !FileSystem::Exists(m_LastOutputDirectory));
        if (ImGui::Button("Open Folder", ImVec2(120.0f, 0.0f)))
        {
            PlatformUtils::OpenPathInExplorer(m_LastOutputDirectory);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_Open = false;
            ImGui::CloseCurrentPopup();
        }

        if (!m_StatusMessage.empty())
        {
            ImGui::TextWrapped("%s%s", m_StatusIsError ? "Error: " : "", m_StatusMessage.c_str());
        }

        if (m_HasReport && ImGui::CollapsingHeader("Export Details"))
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
