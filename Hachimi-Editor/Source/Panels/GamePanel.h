#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/FrameBuffer.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    struct EditorContext;

    // Runtime game view rendered from the scene's primary camera.
    class GamePanel
    {
    public:
        GamePanel();

        void RenderScene(EditorContext& context);
        void Draw(EditorContext& context);

    private:
        Ref<Framebuffer> m_SceneFramebuffer;
        Ref<Framebuffer> m_DisplayFramebuffer;
    };
}
