#include "Renderer/RendererAPI.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace HachimiEngine
{
    RendererAPIType RendererAPI::GetAPI()
    {
        return RendererAPIType::OpenGL;
    }

    Scope<RendererAPI> RendererAPI::Create()
    {
        return CreateScope<OpenGLRendererAPI>();
    }
}
