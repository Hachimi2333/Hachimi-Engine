#include "Renderer/ShadowMap.h"

#include "Platform/OpenGL/OpenGLShadowMap.h"

namespace HachimiEngine
{
    Ref<ShadowMap> ShadowMap::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<OpenGLShadowMap>(width, height);
    }
}
