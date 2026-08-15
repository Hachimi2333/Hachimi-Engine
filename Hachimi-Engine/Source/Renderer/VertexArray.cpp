#include "Renderer/VertexArray.h"

#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace HachimiEngine
{
    Ref<VertexArray> VertexArray::Create()
    {
        return CreateRef<OpenGLVertexArray>();
    }
}
