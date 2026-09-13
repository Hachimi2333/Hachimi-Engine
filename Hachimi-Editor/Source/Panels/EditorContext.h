#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <functional>
#include <filesystem>
#include <string>

#include <imgui.h>
#include <ImGuizmo.h>

namespace HachimiEngine
{
    class AssetDatabase;
    class CommandHistory;
    class SceneDirtyState;
    class TextureCache;

    // Runtime simulation state controlled by the viewport playback toolbar.
    enum class EditorPlayState
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2
    };

    // An action that has to wait for the unsaved-changes prompt before it runs: switching scene,
    // returning to the project hub, closing the window.
    struct EditorPendingAction
    {
        std::string Label;
        std::function<void()> Run;

        bool IsValid() const { return Run != nullptr; }
        void Clear()
        {
            Label.clear();
            Run = nullptr;
        }
    };

    // Shared state passed to the editor panels owned by EditorLayer.
    struct EditorContext
    {
        // ActiveScene is the live scene being edited; EditorScene holds the pre-play scene.
        Ref<Scene> ActiveScene;
        Ref<Scene> EditorScene;
        Entity SelectedEntity;

        // Selected content-browser asset, if any. The inspector shows one or the other, never both.
        AssetHandle SelectedAsset;
        std::filesystem::path SelectedAssetPath;

        EditorCamera Camera;

        // Services the panels read. Owned by EditorLayer (the history and the dirty state) or by
        // the application (the asset services), never by a panel.
        CommandHistory* History = nullptr;
        SceneDirtyState* DirtyState = nullptr;
        AssetDatabase* Assets = nullptr;
        TextureCache* Textures = nullptr;

        EditorPendingAction PendingAction;

        Math::Vec2 ViewportSize { 1280.0f, 720.0f };
        Math::Vec2 GameViewportSize { 1280.0f, 720.0f };
        bool ViewportHovered = false;
        bool ViewportFocused = false;

        bool FocusGamePanel = false;
        bool FocusViewportPanel = false;

        ImGuizmo::OPERATION GizmoOperation = ImGuizmo::TRANSLATE;
        EditorPlayState PlayState = EditorPlayState::Stopped;

        // True while the editor is in Play mode, where edits belong to the runtime copy and must
        // not touch the scene being edited or its undo history.
        bool IsPlaying() const { return PlayState != EditorPlayState::Stopped; }

        // Selecting an entity and selecting an asset are the same choice: the Inspector shows one
        // or the other, so making one selection has to clear the other. Every selection in the
        // editor goes through these two so the two states cannot both be set.
        void SelectEntity(Entity entity)
        {
            SelectedEntity = entity;
            SelectedAsset = AssetHandle::Invalid();
            SelectedAssetPath.clear();
        }

        void SelectAsset(AssetHandle handle, const std::filesystem::path& path)
        {
            SelectedAsset = handle;
            SelectedAssetPath = path;
            SelectedEntity = {};
        }
    };
}
