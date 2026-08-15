#include "Renderer/TextureCube.h"

#include "Platform/OpenGL/OpenGLTextureCube.h"

namespace HachimiEngine
{
    Ref<TextureCube> TextureCube::Create(uint32_t size, uint32_t mipLevelCount)
    {
        return CreateRef<OpenGLTextureCube>(size, mipLevelCount);
    }
}
