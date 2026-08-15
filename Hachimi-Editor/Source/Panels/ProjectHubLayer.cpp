#include "Panels/ProjectHubLayer.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "Utils/FileDialogs.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <cstdio>

namespace HachimiEngine
{
    ProjectHubLayer::ProjectHubLayer()
        : Layer("ProjectHubLayer")
    {
    }

    void ProjectHubLayer::OnAttach()
    {
        ProjectManager::Init();
        std::snprintf(m_ProjectLocation, sizeof(m_ProjectLocation), "%s", PlatformUtils::GetDefaultProjectsDirectory().string().c_str());
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
        ImGui::TextUnformatted("Hachimi-Engine Project Hub");

        ImGui::Separator();

        ImGui::TextUnformatted("New Project");
        ImGui::InputText("Project Name", m_ProjectName, sizeof(m_ProjectName));
        ImGui::InputText("Location", m_ProjectLocation, sizeof(m_ProjectLocation));

        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            FileDialogs::OpenDirectoryDialog(m_ProjectLocation);
        }

        std::string selectedDirectory;
        if (FileDialogs::DrawDirectoryDialog(selectedDirectory))
        {
            if (!selectedDirectory.empty())
            {
                std::snprintf(m_ProjectLocation, sizeof(m_ProjectLocation), "%s", selectedDirectory.c_str());
            }
        }

        if (ImGui::Button("Create Project") && m_ProjectName[0] != '\0')
        {
            const Ref<Project> project = ProjectManager::CreateProject(m_ProjectName, m_ProjectLocation);
            if (project != nullptr && !m_EditorPushed)
            {
                const Ref<EditorLayer> editorLayer = CreateRef<EditorLayer>();
                Application::Get().PushLayer(editorLayer);
                m_EditorPushed = true;
                m_EditorLayer = editorLayer.get();
            }
        }

        ImGui::Separator();

        ImGui::TextUnformatted("Recent Projects");
        const std::vector<std::filesystem::path>& recentProjects = ProjectManager::GetRecentProjects();
        for (size_t i = 0; i < recentProjects.size(); ++i)
        {
            const std::filesystem::path& projectPath = recentProjects[i];
            const std::string label = projectPath.filename().string() + "##" + std::to_string(i);

            if (ImGui::Button(label.c_str()))
            {
                OpenProject(projectPath);
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(("Remove##" + std::to_string(i)).c_str()))
            {
                ProjectManager::RemoveRecentProject(projectPath);
            }
        }

        if (recentProjects.empty())
        {
            ImGui::TextDisabled("No recent projects");
        }

        ImGui::Separator();

        if (ImGui::Button("Open Project..."))
        {
            FileDialogs::OpenProjectFileDialog(PlatformUtils::GetUserDocumentsDirectory());
        }

        std::string selectedProjectPath;
        if (FileDialogs::DrawProjectFileDialog(selectedProjectPath))
        {
            if (!selectedProjectPath.empty())
            {
                OpenProject(selectedProjectPath);
            }
        }

        ImGui::End();
    }

    void ProjectHubLayer::OpenProject(const std::filesystem::path& projectFilePath)
    {
        if (ProjectManager::OpenProject(projectFilePath) == nullptr || m_EditorPushed)
        {
            return;
        }

        const Ref<EditorLayer> editorLayer = CreateRef<EditorLayer>();
        Application::Get().PushLayer(editorLayer);
        m_EditorPushed = true;
        m_EditorLayer = editorLayer.get();
    }
}
