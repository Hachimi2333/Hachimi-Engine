#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Panels/PhysicsDebugOverlay.h"
#include "Renderer/SceneRenderTarget.h"
#include "Scene/Entity.h"

#include <imgui.h>

namespace HachimiEngine
{
    class CommandHistory;
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
        void Draw(EditorContext& context, bool* open = nullptr);

    private:
        void ManipulateSelectedEntity(EditorContext& context, const ImVec2& imageMin, const ImVec2& imageMax);

    private:
        RendererContext* m_Renderer = nullptr;
        Scope<SceneRenderer> m_SceneRenderer;
        Scope<SceneRenderTarget> m_Target;
        // Physically stateless - it only needs the debug renderer for the duration of a draw - so it
        // is a value rather than a pointer.
        PhysicsDebugOverlay m_PhysicsOverlay;

        // Gizmo drag state: the entity being manipulated and the history its edits belong to. The
        // history is refreshed when the selection changes, which is what makes one drag one undo
        // entry even though the gizmo reports a result on every frame of it.
        Entity m_GizmoEntity;
        CommandHistory* m_GizmoHistory = nullptr;
    };
}
