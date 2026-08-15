#include "Panels/EditorLayer.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/Scene.h"

#include <ImGuizmo.h>
#include <imgui.h>

namespace HachimiEngine
{
    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
    {
    }

    void EditorLayer::OnAttach()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            HE_CLIENT_ERROR("Cannot open editor without an active project");
            return;
        }

        AssetManager::Init(project->GetAssetsDirectory());
        m_Context.Scene = project->GetActiveScene();
        m_Context.Camera.SetViewportSize(Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight());
        m_ConsolePanel.RegisterCallbacks();

        HE_CLIENT_INFO("Editing project: {}", project->GetName());
    }

    void EditorLayer::OnDetach()
    {
        m_ConsolePanel.UnregisterCallbacks();
        AssetManager::Shutdown();
    }

    void EditorLayer::OnUpdate(Timestep timestep)
    {
        m_Context.Camera.OnUpdate(timestep);
        if (m_Context.Scene != nullptr)
        {
            m_Context.Scene->OnUpdate(timestep);
        }
    }

    void EditorLayer::OnImGuiRender()
    {
        ImGuizmo::BeginFrame();
        m_ViewportPanel.RenderScene(m_Context);

        DrawDockSpace();
        m_MenuBar.Draw(this, m_Context);
        m_SceneHierarchyPanel.Draw(m_Context);
        m_InspectorPanel.Draw(m_Context);
        m_ContentBrowserPanel.Draw(m_Context);
        m_ConsolePanel.Draw();
        m_ViewportPanel.Draw(m_Context);
    }

    void EditorLayer::OnEvent(Event& event)
    {
        // Panels mostly poll ImGui state; additional event handling can be added here.
    }

    void EditorLayer::DrawDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
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

        ImGui::Begin("EditorDockSpace", nullptr, windowFlags);
        ImGui::DockSpace(ImGui::GetID("EditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
}
