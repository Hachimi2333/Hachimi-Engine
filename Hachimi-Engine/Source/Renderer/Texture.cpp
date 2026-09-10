#include "Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace HachimiEngine
{
    Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
    {
        return CreateRef<OpenGLTexture2D>(specification);
    }

    Ref<Texture2D> Texture2D::Create(const DecodedImage& image)
    {
        if (!image.IsValid())
        {
            return nullptr;
        }

        TextureSpecification specification;
        specification.Width = image.Width;
        specification.Height = image.Height;
        specification.Channels = 4;
        specification.SRGB = true;
        specification.GenerateMips = true;

        Ref<Texture2D> texture = CreateRef<OpenGLTexture2D>(specification);
        if (texture->GetRendererID() == 0)
        {
            return nullptr;
        }

        texture->SetData(const_cast<uint8_t*>(image.Pixels.data()),
            static_cast<uint32_t>(image.Pixels.size()));
        return texture;
    }

    Ref<Texture2D> Texture2D::Create(const std::string& path)
    {
        Ref<Texture2D> texture = CreateRef<OpenGLTexture2D>(path);
        if (texture->GetRendererID() == 0)
        {
            // Loading failed; report it as a missing texture rather than handing
            // out a texture with no GPU resource.
            return nullptr;
        }
        return texture;
    }
}
