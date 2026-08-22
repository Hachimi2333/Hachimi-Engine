#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorMenuBar.h"
#include "Panels/GamePanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ToolbarPanel.h"
#include "Panels/ViewportPanel.h"
#include "Renderer/EditorCamera.h"

namespace HachimiEngine
{
    class Scene;

    // Main editor layer; owns the shared context and all docking panels.
    class EditorLayer final : public Layer
    {
    public:
        EditorLayer();
        ~EditorLayer() override = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(Timestep timestep) override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

        // Playback controls used by the toolbar panel.
        void OnPlay();
        void OnPause();
        void OnStop();

        // Rebuilds the default docking layout at the start of the next frame.
        void ResetLayout();

    private:
        void DrawDockSpace();

    private:
        EditorContext m_Context;
        ViewportPanel m_ViewportPanel;
        GamePanel m_GamePanel;
        SceneHierarchyPanel m_SceneHierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        ConsolePanel m_ConsolePanel;
        EditorMenuBar m_MenuBar;
        ToolbarPanel m_ToolbarPanel;
        bool m_ResetLayoutRequested = false;
    };
}
