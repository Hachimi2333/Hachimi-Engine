#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/FrameBuffer.h"

#include <imgui.h>

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
        void ManipulateSelectedEntity(EditorContext& context, const ImVec2& imageMin, const ImVec2& imageMax);

    private:
        Ref<Framebuffer> m_SceneFramebuffer;
        Ref<Framebuffer> m_DisplayFramebuffer;
    };
}
