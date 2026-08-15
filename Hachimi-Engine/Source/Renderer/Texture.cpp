#include "Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace HachimiEngine
{
    Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
    {
        return CreateRef<OpenGLTexture2D>(specification);
    }

    Ref<Texture2D> Texture2D::Create(const std::string& path)
    {
        return CreateRef<OpenGLTexture2D>(path);
    }
}
