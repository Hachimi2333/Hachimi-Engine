#pragma once

#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace HachimiEngine
{
    // Runtime simulation state controlled by the viewport playback toolbar.
    enum class EditorPlayState
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2
    };

    // Shared state passed to the editor panels owned by EditorLayer.
    struct EditorContext
    {
        // ActiveScene is the live scene being edited; EditorScene holds the pre-play scene.
        Ref<Scene> ActiveScene;
        Ref<Scene> EditorScene;
        Entity SelectedEntity;
        EditorCamera Camera;

        Math::Vec2 ViewportSize { 1280.0f, 720.0f };
        Math::Vec2 GameViewportSize { 1280.0f, 720.0f };
        bool ViewportHovered = false;
        bool ViewportFocused = false;

        bool FocusGamePanel = false;
        bool FocusViewportPanel = false;

        ImGuizmo::OPERATION GizmoOperation = ImGuizmo::TRANSLATE;
        EditorPlayState PlayState = EditorPlayState::Stopped;
    };
}
