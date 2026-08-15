#pragma once

#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>

namespace HachimiEngine
{
    // Shared state passed to the editor panels owned by EditorLayer.
    struct EditorContext
    {
        Ref<Scene> Scene;
        Entity SelectedEntity;
        EditorCamera Camera;

        glm::vec2 ViewportSize { 1280.0f, 720.0f };
        bool ViewportHovered = false;
        bool ViewportFocused = false;

        ImGuizmo::OPERATION GizmoOperation = ImGuizmo::TRANSLATE;
    };
}
