#include "Renderer/Buffer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace HachimiEngine
{
    Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
    {
        return CreateRef<OpenGLVertexBuffer>(size);
    }

    Ref<VertexBuffer> VertexBuffer::Create(const float* vertices, uint32_t size)
    {
        return CreateRef<OpenGLVertexBuffer>(vertices, size);
    }

    Ref<IndexBuffer> IndexBuffer::Create(const uint32_t* indices, uint32_t count)
    {
        return CreateRef<OpenGLIndexBuffer>(indices, count);
    }
}
