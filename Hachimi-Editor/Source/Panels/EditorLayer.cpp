#include "Panels/EditorLayer.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/Scene.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace HachimiEngine
{
    namespace
    {
        // Builds the initial editor docking layout the first time the dock space appears.
        void SetupDefaultEditorDockLayout(ImGuiID dockspaceId, const ImVec2& dockspaceSize)
        {
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, dockspaceSize);

            ImGuiID dockMain = dockspaceId;
            ImGuiID dockBottom = 0;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, &dockBottom, &dockMain);

            ImGuiID dockRight = 0;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, &dockRight, &dockMain);

            ImGuiID dockLeft = 0;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, &dockLeft, &dockMain);

            ImGuiID dockConsole = 0;
            ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Right, 0.5f, &dockConsole, &dockBottom);

            ImGui::DockBuilderDockWindow("Viewport", dockMain);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
            ImGui::DockBuilderDockWindow("Inspector", dockRight);
            ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
            ImGui::DockBuilderDockWindow("Console", dockConsole);
            ImGui::DockBuilderFinish(dockspaceId);
        }
    }

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
        // Do not move the camera while the user is typing into an ImGui input field.
        if (!ImGui::GetIO().WantCaptureKeyboard)
        {
            m_Context.Camera.OnUpdate(timestep);
        }

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
        const ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");

        // Build the layout once, before the dock space is submitted for this frame.
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
        {
            SetupDefaultEditorDockLayout(dockspaceId, viewport->WorkSize);
        }

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
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
}
