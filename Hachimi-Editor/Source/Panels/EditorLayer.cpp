#include "Panels/EditorLayer.h"

#include "Asset/AssetDatabase.h"
#include "Asset/MaterialAsset.h"
#include "Asset/TextureCache.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Events/EventDispatcher.h"
#include "Panels/EditorShortcuts.h"
#include "Project/ProjectManager.h"
#include "Renderer/RendererContext.h"
#include "Scene/Scene.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/FileDialogs.h"
#include "Utils/FileSystem.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <utility>

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
            ImGui::DockBuilderDockWindow("Package Inspector", dockConsole);
            ImGui::DockBuilderFinish(dockspaceId);
        }

        // A name that is free in the given directory, so "New Scene" twice produces two scenes
        // instead of one silently overwriting the other.
        std::filesystem::path FindUniqueScenePath(const std::filesystem::path& directory, const std::string& stem)
        {
            std::filesystem::path candidate = directory / (stem + ".hscene");
            for (int suffix = 1; FileSystem::Exists(candidate); ++suffix)
            {
                candidate = directory / (stem + "_" + std::to_string(suffix) + ".hscene");
            }
            return candidate;
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

        Application& application = Application::Get();

        // The asset database scans before the scene loads: a scene resolves the material and script
        // references it carries, which needs the identity sidecars to be readable.
        AssetDatabase& assets = application.GetAssetDatabase();
        TextureCache& textures = application.GetTextureCache();
        assets.Refresh(project->GetAssetsDirectory());
        textures.Clear();
        textures.SetDatabase(&assets);

        project->SetAssetDatabase(&assets);
        SceneSerializer::SetAssetDatabase(&assets);

        if (!ProjectManager::EnsureStartScene(project))
        {
            HE_CLIENT_ERROR("Could not open the start scene of project '{}'", project->GetName());
        }

        m_Context.Assets = &assets;
        m_Context.Textures = &textures;
        m_Context.DirtyState = &m_DirtyState;
        BindActiveScene(project->GetActiveScene());
        m_Context.EditorScene = nullptr;
        m_Context.PlayState = EditorPlayState::Stopped;
        m_Context.Camera.SetViewportSize(application.GetWindow().GetWidth(), application.GetWindow().GetHeight());
        m_ConsolePanel.RegisterCallbacks();

        // Both panels render the same scene from different cameras, so each gets its own
        // SceneRenderer; only the GPU resources behind them are shared.
        RendererContext& rendererContext = application.GetRendererContext();
        m_ViewportPanel.Init(rendererContext);
        m_GamePanel.Init(rendererContext);

        HE_CLIENT_INFO("Editing project: {}", project->GetName());
    }

    void EditorLayer::OnDetach()
    {
        if (m_Context.ActiveScene != nullptr && m_Context.PlayState != EditorPlayState::Stopped)
        {
            m_Context.ActiveScene->OnRuntimeStop();
        }

        m_ConsolePanel.UnregisterCallbacks();

        // The services belong to the application, so the layer only drops its references.
        m_Context.History = nullptr;
        m_Context.DirtyState = nullptr;
        m_Context.Assets = nullptr;
        m_Context.Textures = nullptr;
    }

    void EditorLayer::BindActiveScene(const Ref<Scene>& scene)
    {
        // The context is what every panel reads, so binding the scene means both storing it and
        // rebuilding the per-scene state that follows it. Without the assignment here the editor
        // renders with no scene at all: a project opens and loads its start scene, and then every
        // panel reports that nothing is open.
        m_Context.ActiveScene = scene;
        m_Context.SelectEntity({});
        m_Context.History = nullptr;
        m_History.reset();
        m_DirtyState.Bind(scene);

        if (scene == nullptr)
        {
            return;
        }

        m_History = CreateScope<CommandHistory>(*scene);
        m_Context.History = m_History.get();
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

        // Thumbnails and any other deferred texture work. The upload has to happen on the main
        // thread, where the GL context is current.
        if (m_Context.Textures != nullptr)
        {
            m_Context.Textures->PumpCompletedRequests();
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

        // The clone is a different scene object, so undo works on it and never rewinds an edit the
        // user made before pressing Play.
        BindActiveScene(m_Context.ActiveScene);

        if (m_Context.SelectedEntity)
        {
            const UUID selectedUUID = m_Context.SelectedEntity.GetUUID();
            m_Context.SelectEntity(m_Context.ActiveScene->GetEntityByUUID(selectedUUID));
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
            BindActiveScene(m_Context.ActiveScene);
            m_Context.SelectEntity(m_Context.ActiveScene->GetEntityByUUID(selectedUUID));
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
        m_PackageInspectorPanel.Draw(this, m_Context);
        m_BuildSettingsPanel.Draw(this, m_Context);

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

        // Shortcuts and the save prompt run last, so they see the selection this frame's panels
        // just produced.
        EditorShortcuts::Handle(*this, m_Context);
        DrawSavePrompt();
    }

    void EditorLayer::DrawSavePrompt()
    {
        switch (m_SavePrompt.Draw())
        {
            case SavePromptPopup::Choice::Save:
            {
                if (!SaveActiveScene())
                {
                    HE_CLIENT_ERROR("Could not save the scene; the pending action was cancelled");
                    m_Context.PendingAction.Clear();
                    break;
                }

                if (m_Context.PendingAction.IsValid())
                {
                    std::function<void()> action = std::move(m_Context.PendingAction.Run);
                    m_Context.PendingAction.Clear();
                    action();
                }
                break;
            }
            case SavePromptPopup::Choice::Discard:
            {
                if (m_Context.PendingAction.IsValid())
                {
                    std::function<void()> action = std::move(m_Context.PendingAction.Run);
                    m_Context.PendingAction.Clear();
                    action();
                }
                break;
            }
            case SavePromptPopup::Choice::Cancel:
            {
                m_Context.PendingAction.Clear();
                // A cancelled close must not leave the window thinking it is on its way out.
                m_CloseConfirmed = false;
                break;
            }
            case SavePromptPopup::Choice::None:
            default:
                break;
        }
    }

    void EditorLayer::RequestAction(const std::string& label, std::function<void()> action)
    {
        if (action == nullptr)
        {
            return;
        }

        if (!m_DirtyState.IsDirty())
        {
            action();
            return;
        }

        m_Context.PendingAction.Label = label;
        m_Context.PendingAction.Run = std::move(action);
        m_SavePrompt.Open(label);
    }

    void EditorLayer::SwitchToScene(const std::filesystem::path& scenePath)
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            return;
        }

        RequestAction("opening another scene", [this, project, scenePath]
        {
            if (m_Context.PlayState != EditorPlayState::Stopped)
            {
                OnStop();
            }

            if (!project->OpenScene(scenePath))
            {
                HE_CLIENT_ERROR("Could not open scene {}", scenePath.string());
                return;
            }

            m_Context.EditorScene = nullptr;
            m_Context.PlayState = EditorPlayState::Stopped;
            BindActiveScene(project->GetActiveScene());
            HE_CLIENT_INFO("Opened scene {}", scenePath.string());
        });
    }

    bool EditorLayer::SaveActiveScene()
    {
        Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            return false;
        }

        // Only the scene being edited is written: in Play mode the project's active scene is the
        // throwaway clone, so the edit scene is handed to the project first.
        const Ref<Scene> scene = m_Context.PlayState == EditorPlayState::Stopped
            ? m_Context.ActiveScene
            : m_Context.EditorScene;
        if (scene == nullptr)
        {
            return false;
        }

        if (project->GetActiveScene() != scene)
        {
            project->SetActiveScene(scene);
        }

        // The history's scene reference has to follow, or an undo after saving would write into a
        // scene nobody is editing any more.
        if (m_History == nullptr || &m_History->GetScene() != scene.get())
        {
            BindActiveScene(scene);
        }

        if (!project->SaveActiveScene())
        {
            HE_CLIENT_ERROR("Failed to save the active scene");
            return false;
        }

        // The serializer cleared the scene's flag; reading it back keeps the wrapper's revision
        // in step so the save prompt closes.
        m_DirtyState.IsDirty();
        HE_CLIENT_INFO("Saved scene {}", project->GetActiveScenePath().string());
        return true;
    }

    bool EditorLayer::SaveActiveSceneAs(const std::filesystem::path& scenePath)
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            return false;
        }

        if (!project->SaveActiveSceneAs(scenePath))
        {
            HE_CLIENT_ERROR("Failed to save the scene as {}", scenePath.string());
            return false;
        }

        m_DirtyState.IsDirty();
        HE_CLIENT_INFO("Saved scene as {}", scenePath.string());
        return true;
    }

    void EditorLayer::SaveSceneAsWithDialog()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            return;
        }

        const std::filesystem::path selectedPath = FileDialogs::SaveFileDialog(
            project->GetAssetsDirectory() / "Scenes",
            project->GetActiveScenePath().filename().string());
        if (!selectedPath.empty())
        {
            SaveActiveSceneAs(selectedPath);
        }
    }

    void EditorLayer::OpenSceneWithDialog()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr || m_Context.Assets == nullptr)
        {
            return;
        }

        const std::filesystem::path selectedScenePath =
            FileDialogs::OpenSceneFileDialog(m_Context.Assets->GetAssetsDirectory() / "Scenes");
        if (!selectedScenePath.empty())
        {
            SwitchToScene(selectedScenePath);
        }
    }

    void EditorLayer::CreateNewScene()
    {
        Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr || m_Context.Assets == nullptr)
        {
            return;
        }

        // The scene is written first and identified second: the database mints the identity from the
        // file that is actually there, so the record and the contents cannot disagree about which
        // scene this is.
        const std::filesystem::path scenePath =
            FindUniqueScenePath(m_Context.Assets->GetAssetsDirectory() / "Scenes", "NewScene");

        const Ref<Scene> scene = CreateRef<Scene>();
        scene->SetName("NewScene");

        SceneSerializer serializer(scene);
        if (!serializer.Serialize(scenePath.string()))
        {
            HE_CLIENT_ERROR("Could not write the new scene {}", scenePath.string());
            return;
        }

        m_Context.Assets->Refresh(m_Context.Assets->GetAssetsDirectory());
        SwitchToScene(scenePath);
    }

    void EditorLayer::CreateNewMaterial()
    {
        if (m_Context.Assets == nullptr)
        {
            return;
        }

        AssetHandle handle;
        const AssetWriteResult created = m_Context.Assets->CreateAsset(
            m_Context.Assets->GetAssetsDirectory() / "Materials",
            "NewMaterial",
            AssetType::Material,
            MaterialAsset::Serialize(MaterialAsset::Default()),
            handle);

        if (created != AssetWriteResult::Success)
        {
            HE_CLIENT_ERROR("Could not create a material: {}", ToString(created));
            return;
        }

        m_Context.SelectAsset(handle, m_Context.Assets->GetAssetPath(handle));
        HE_CLIENT_INFO("Created material {}", m_Context.SelectedAssetPath.string());
    }

    void EditorLayer::RequestClose()
    {
        if (m_CloseConfirmed || !m_DirtyState.IsDirty())
        {
            m_CloseConfirmed = true;
            Application::Get().Close();
            return;
        }

        RequestAction("closing the editor", []
        {
            Application::Get().Close();
        });
    }

    void EditorLayer::ResetLayout()
    {
        m_ResetLayoutRequested = true;
    }

    void EditorLayer::OpenBuildSettings()
    {
        m_BuildSettingsPanel.Open();
    }

    void EditorLayer::OnEvent(Event& event)
    {
        // Closing with unsaved changes asks first. Consuming the event stops Application from
        // shutting the editor down behind the prompt's back.
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& closeEvent)
        {
            if (m_CloseConfirmed || !m_DirtyState.IsDirty())
            {
                m_CloseConfirmed = true;
                return false;
            }

            RequestClose();
            closeEvent.Handled = true;
            return true;
        });
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
