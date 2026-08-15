#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/FrameBuffer.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    struct EditorContext;

    // Framebuffer viewport with EditorCamera controls and ImGuizmo manipulation.
    class ViewportPanel
    {
    public:
        ViewportPanel();

        void RenderScene(EditorContext& context);
        void Draw(EditorContext& context);

    private:
        void DrawGizmoToolbar(EditorContext& context);
        void ManipulateSelectedEntity(EditorContext& context);

    private:
        Ref<Framebuffer> m_Framebuffer;
    };
}
