#include "Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace HachimiEngine
{
    Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
    {
        return CreateRef<OpenGLTexture2D>(specification);
    }

    Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification, const DecodedImage& image)
    {
        if (!image.IsValid())
        {
            return nullptr;
        }

        // The image supplies the dimensions; everything else stays as the caller asked for it,
        // which is what carries import settings (sRGB, mipmaps, wrap, filter) to the GPU.
        TextureSpecification resolved = specification;
        resolved.Width = image.Width;
        resolved.Height = image.Height;
        resolved.Channels = 4;

        Ref<Texture2D> texture = CreateRef<OpenGLTexture2D>(resolved);
        if (texture->GetRendererID() == 0)
        {
            return nullptr;
        }

        texture->SetData(const_cast<uint8_t*>(image.Pixels.data()),
            static_cast<uint32_t>(image.Pixels.size()));
        return texture;
    }

    Ref<Texture2D> Texture2D::Create(const DecodedImage& image)
    {
        return Create(TextureSpecification(), image);
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
