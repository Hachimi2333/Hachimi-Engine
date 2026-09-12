#include "Renderer/UniformBuffer.h"

#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace HachimiEngine
{
    Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t bindingPoint)
    {
        return CreateRef<OpenGLUniformBuffer>(size, bindingPoint);
    }
}
