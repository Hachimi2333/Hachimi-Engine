#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/FrameBuffer.h"

namespace HachimiEngine
{
    class RendererContext;
    class SceneRenderer;
    struct EditorContext;

    // Runtime game view rendered from the scene's primary camera.
    class GamePanel
    {
    public:
        GamePanel();
        // Defined out of line: the panels own a SceneRenderer, and the destructor needs
        // its complete type.
        ~GamePanel();

        GamePanel(const GamePanel&) = delete;
        GamePanel& operator=(const GamePanel&) = delete;

        // Binds the panel to the shared renderer resources. Requires a current GL context.
        void Init(RendererContext& rendererContext);

        void RenderScene(EditorContext& context);
        void Draw(EditorContext& context);

    private:
        RendererContext* m_Renderer = nullptr;
        Scope<SceneRenderer> m_SceneRenderer;
        Ref<Framebuffer> m_SceneFramebuffer;
        Ref<Framebuffer> m_DisplayFramebuffer;
    };
}
