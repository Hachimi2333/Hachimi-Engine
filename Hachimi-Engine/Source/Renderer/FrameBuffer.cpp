#include "Renderer/FrameBuffer.h"

#include "Platform/OpenGL/OpenGLFrameBuffer.h"

namespace HachimiEngine
{
    Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& specification)
    {
        return CreateRef<OpenGLFramebuffer>(specification);
    }
}
