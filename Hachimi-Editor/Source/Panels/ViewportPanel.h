#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/SceneRenderTarget.h"

#include <imgui.h>

namespace HachimiEngine
{
    class RendererContext;
    class SceneRenderer;
    struct EditorContext;

    // Framebuffer viewport with EditorCamera controls and ImGuizmo manipulation.
    class ViewportPanel
    {
    public:
        ViewportPanel();
        // Defined out of line: the panels own a SceneRenderer, and the destructor needs
        // its complete type.
        ~ViewportPanel();

        ViewportPanel(const ViewportPanel&) = delete;
        ViewportPanel& operator=(const ViewportPanel&) = delete;

        // Binds the panel to the shared renderer resources. Requires a current GL context.
        void Init(RendererContext& rendererContext);

        void RenderScene(EditorContext& context);
        void Draw(EditorContext& context);

    private:
        void ManipulateSelectedEntity(EditorContext& context, const ImVec2& imageMin, const ImVec2& imageMax);

    private:
        RendererContext* m_Renderer = nullptr;
        Scope<SceneRenderer> m_SceneRenderer;
        Scope<SceneRenderTarget> m_Target;
    };
}
