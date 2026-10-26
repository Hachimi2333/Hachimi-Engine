#include "Panels/ProjectHubLayer.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "Utils/FileDialogs.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <cstdio>
#include <utility>

namespace HachimiEngine
{
    ProjectHubLayer::ProjectHubLayer(std::filesystem::path projectFile)
        : Layer("ProjectHubLayer")
        , m_StartupProjectFile(std::move(projectFile))
    {
    }

    void ProjectHubLayer::OnAttach()
    {
        ProjectManager::Init();
        std::snprintf(m_ProjectLocation, sizeof(m_ProjectLocation), "%s", PlatformUtils::GetDefaultProjectsDirectory().string().c_str());

        // "--project" hands the editor a project to open, so a launch from the command line or a
        // shortcut does not have to go through the hub.
        if (!m_StartupProjectFile.empty())
        {
            OpenProject(m_StartupProjectFile);
            m_StartupProjectFile.clear();
        }
    }

    void ProjectHubLayer::OnImGuiRender()
    {
        // The editor may have returned to the hub by popping itself.
        if (m_EditorPushed && (m_EditorLayer == nullptr || !Application::Get().HasLayer(m_EditorLayer)))
        {
            m_EditorPushed = false;
            m_EditorLayer = nullptr;
        }

        // While the editor is active, keep the hub in the layer stack but do not render it.
        if (m_EditorPushed)
        {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("Project Hub", nullptr, windowFlags);
        ImGui::TextUnformatted("Hachimi-Engine");
        ImGui::TextDisabled("Open a project to start editing.");
        if (ImGui::Button("Open Project..."))
        {
            const std::filesystem::path selectedProjectPath =
                FileDialogs::OpenProjectFileDialog(PlatformUtils::GetUserDocumentsDirectory());
            if (!selectedProjectPath.empty())
            {
                OpenProject(selectedProjectPath);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("New Project...")) { m_Status.clear(); ImGui::OpenPopup("New Project"); }
        if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {

            ImGui::InputText("Project Name", m_ProjectName, sizeof(m_ProjectName));
            ImGui::InputText("Location", m_ProjectLocation, sizeof(m_ProjectLocation));

            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const std::filesystem::path selectedDirectory =
                    FileDialogs::OpenDirectoryDialog(m_ProjectLocation);
                if (!selectedDirectory.empty())
                {
                    std::snprintf(m_ProjectLocation, sizeof(m_ProjectLocation), "%s", selectedDirectory.string().c_str());
                }
            }

            if (ImGui::Button("Create") && m_ProjectName[0] != '\0')
            {
                const Ref<Project> project = ProjectManager::CreateProject(m_ProjectName, m_ProjectLocation);
                if (project != nullptr && !m_EditorPushed)
                {
                    const Ref<EditorLayer> editorLayer = CreateRef<EditorLayer>();
                    Application::Get().PushLayer(editorLayer);
                    m_EditorPushed = true;
                    m_EditorLayer = editorLayer.get();
                    ImGui::CloseCurrentPopup();
                }
                else if (project == nullptr) m_Status = "Could not create project. Check the name, location and existing folders.";
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();

            if (!m_Status.empty()) ImGui::TextWrapped("%s", m_Status.c_str());
            ImGui::EndPopup();
        }
        if (!m_Status.empty()) ImGui::TextWrapped("%s", m_Status.c_str());
        ImGui::Separator();
        ImGui::TextUnformatted("Recent Projects");
        const std::vector<std::filesystem::path> recentProjects = ProjectManager::GetRecentProjects();
        for (size_t i = 0; i < recentProjects.size(); ++i)
        {
            const std::filesystem::path& projectPath = recentProjects[i];
            const std::string label = projectPath.filename().string() + "##" + std::to_string(i);

            if (ImGui::Button(label.c_str()))
            {
                OpenProject(projectPath);
            }

            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", projectPath.string().c_str());
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove from recent projects"))
                {
                    ProjectManager::RemoveRecentProject(projectPath);
                }
            ImGui::EndPopup();
            }
        }

        if (recentProjects.empty())
        {
            ImGui::TextDisabled("No recent projects");
        }

        ImGui::End();
    }

    void ProjectHubLayer::OpenProject(const std::filesystem::path& projectFilePath)
    {
        if (ProjectManager::OpenProject(projectFilePath) == nullptr || m_EditorPushed)
        {
            m_Status = "Could not open the project. Check that the file exists and is valid.";
            return;
        }

        m_Status.clear();
        const Ref<EditorLayer> editorLayer = CreateRef<EditorLayer>();
        Application::Get().PushLayer(editorLayer);
        m_EditorPushed = true;
        m_EditorLayer = editorLayer.get();
    }
}
