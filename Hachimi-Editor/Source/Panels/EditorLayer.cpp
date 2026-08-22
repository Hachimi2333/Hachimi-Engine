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

            // Split the central area vertically so the toolbar stays above the viewport tabs.
            ImGuiID dockToolbar = 0;
            ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.08f, &dockToolbar, &dockMain);

            ImGui::DockBuilderDockWindow("Toolbar", dockToolbar);
            ImGui::DockBuilderDockWindow("Viewport", dockMain);
            ImGui::DockBuilderDockWindow("Game", dockMain);
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
        m_Context.ActiveScene = project->GetActiveScene();
        m_Context.EditorScene = nullptr;
        m_Context.PlayState = EditorPlayState::Stopped;
        m_Context.Camera.SetViewportSize(Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight());
        m_ConsolePanel.RegisterCallbacks();

        HE_CLIENT_INFO("Editing project: {}", project->GetName());
    }

    void EditorLayer::OnDetach()
    {
        if (m_Context.ActiveScene != nullptr && m_Context.PlayState != EditorPlayState::Stopped)
        {
            m_Context.ActiveScene->OnRuntimeStop();
        }

        m_ConsolePanel.UnregisterCallbacks();
        AssetManager::Shutdown();
    }

    void EditorLayer::OnUpdate(Timestep timestep)
    {
        // Do not move the camera while the user is typing into an ImGui input field.
        // ViewportHovered is updated by the previous frame's viewport panel layout.
        if (!ImGui::GetIO().WantCaptureKeyboard)
        {
            m_Context.Camera.OnUpdate(timestep, m_Context.ViewportHovered && !ImGuizmo::IsUsingAny());
        }

        // Scene simulation only advances while the playback toolbar is in Play mode.
        if (m_Context.ActiveScene != nullptr && m_Context.PlayState == EditorPlayState::Playing)
        {
            m_Context.ActiveScene->OnUpdate(timestep);
        }
    }

    void EditorLayer::OnPlay()
    {
        if (m_Context.ActiveScene == nullptr)
        {
            return;
        }

        if (m_Context.PlayState == EditorPlayState::Paused)
        {
            m_Context.PlayState = EditorPlayState::Playing;
            m_Context.FocusGamePanel = true;
            return;
        }

        if (m_Context.PlayState == EditorPlayState::Playing)
        {
            return;
        }

        // Clone the editor scene so Play mode edits are discarded when stopping.
        m_Context.EditorScene = m_Context.ActiveScene;
        m_Context.ActiveScene = m_Context.EditorScene->Clone();
        m_Context.ActiveScene->OnRuntimeStart();

        if (m_Context.SelectedEntity)
        {
            const UUID selectedUUID = m_Context.SelectedEntity.GetUUID();
            m_Context.SelectedEntity = m_Context.ActiveScene->GetEntityByUUID(selectedUUID);
        }

        m_Context.PlayState = EditorPlayState::Playing;
        m_Context.FocusGamePanel = true;
        m_Context.FocusViewportPanel = false;
    }

    void EditorLayer::OnPause()
    {
        if (m_Context.PlayState == EditorPlayState::Playing)
        {
            m_Context.PlayState = EditorPlayState::Paused;
        }
        else if (m_Context.PlayState == EditorPlayState::Paused)
        {
            m_Context.PlayState = EditorPlayState::Playing;
            m_Context.FocusGamePanel = true;
        }
    }

    void EditorLayer::OnStop()
    {
        if (m_Context.PlayState == EditorPlayState::Stopped)
        {
            return;
        }

        m_Context.PlayState = EditorPlayState::Stopped;

        if (m_Context.ActiveScene != nullptr)
        {
            m_Context.ActiveScene->OnRuntimeStop();
        }

        if (m_Context.EditorScene != nullptr)
        {
            UUID selectedUUID = UUID::Invalid();
            if (m_Context.SelectedEntity)
            {
                selectedUUID = m_Context.SelectedEntity.GetUUID();
            }

            m_Context.ActiveScene = m_Context.EditorScene;
            m_Context.EditorScene = nullptr;
            m_Context.SelectedEntity = m_Context.ActiveScene->GetEntityByUUID(selectedUUID);
        }

        m_Context.FocusViewportPanel = true;
        m_Context.FocusGamePanel = false;
    }

    void EditorLayer::OnImGuiRender()
    {
        ImGuizmo::BeginFrame();
        m_ViewportPanel.RenderScene(m_Context);
        m_GamePanel.RenderScene(m_Context);

        DrawDockSpace();
        m_MenuBar.Draw(this, m_Context);
        m_ToolbarPanel.Draw(this, m_Context);
        m_SceneHierarchyPanel.Draw(m_Context);
        m_InspectorPanel.Draw(m_Context);
        m_ContentBrowserPanel.Draw(this, m_Context);
        m_ConsolePanel.Draw();

        if (m_Context.FocusGamePanel)
        {
            ImGui::SetNextWindowFocus();
            m_Context.FocusGamePanel = false;
        }
        m_GamePanel.Draw(m_Context);

        if (m_Context.FocusViewportPanel)
        {
            ImGui::SetNextWindowFocus();
            m_Context.FocusViewportPanel = false;
        }
        m_ViewportPanel.Draw(m_Context);
    }

    void EditorLayer::ResetLayout()
    {
        m_ResetLayoutRequested = true;
    }

    void EditorLayer::OnEvent(Event& event)
    {
        // Panels mostly poll ImGui state; additional event handling can be added here.
    }

    void EditorLayer::DrawDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        // Versioned dock space ID gives the new Toolbar/Game panels a clean default layout.
        const ImGuiID dockspaceId = ImGui::GetID("EditorDockSpaceV2");

        // Build the layout once, before the dock space is submitted for this frame. A menu-triggered reset is
        // deferred here because dock builder calls must happen before the dock space is submitted.
        if (m_ResetLayoutRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
        {
            SetupDefaultEditorDockLayout(dockspaceId, viewport->WorkSize);
            m_ResetLayoutRequested = false;
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
