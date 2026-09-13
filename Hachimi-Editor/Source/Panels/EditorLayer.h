#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneDirtyState.h"
#include "Panels/BuildSettingsPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorMenuBar.h"
#include "Panels/GamePanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/PackageInspectorPanel.h"
#include "Panels/SavePromptPopup.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ToolbarPanel.h"
#include "Panels/ViewportPanel.h"
#include "Renderer/EditorCamera.h"

#include <filesystem>
#include <functional>
#include <string>

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

        // Opens the Build Settings export configuration popup.
        void OpenBuildSettings();

        // Runs an action that would discard unsaved changes. When the scene is dirty the action
        // waits for the save prompt instead of running now; label describes it to the user.
        void RequestAction(const std::string& label, std::function<void()> action);

        // Switches the edited scene. Used by the menu, the content browser and the project hub.
        void SwitchToScene(const std::filesystem::path& scenePath);

        // Saves the active scene; returns false when there is nothing to save to.
        bool SaveActiveScene();
        bool SaveActiveSceneAs(const std::filesystem::path& scenePath);
        void SaveSceneAsWithDialog();
        void OpenSceneWithDialog();
        void CreateNewScene();
        void CreateNewMaterial();

    private:
        void DrawDockSpace();
        void DrawSavePrompt();
        void BindActiveScene(const Ref<Scene>& scene);
        void RequestClose();

        // Keeps the menu bar, keyboard shortcuts and file dialogs in one place.
        void DrawShortcuts();

    private:
        EditorContext m_Context;
        ViewportPanel m_ViewportPanel;
        GamePanel m_GamePanel;
        SceneHierarchyPanel m_SceneHierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        ConsolePanel m_ConsolePanel;
        PackageInspectorPanel m_PackageInspectorPanel;
        EditorMenuBar m_MenuBar;
        BuildSettingsPanel m_BuildSettingsPanel;
        ToolbarPanel m_ToolbarPanel;
        SavePromptPopup m_SavePrompt;

        // Owned here because they follow whichever scene is being edited.
        Scope<CommandHistory> m_History;
        SceneDirtyState m_DirtyState;

        bool m_ResetLayoutRequested = false;
        // Set once the user confirmed closing through the save prompt, so OnEvent lets the close
        // through the second time.
        bool m_CloseConfirmed = false;
    };
}
