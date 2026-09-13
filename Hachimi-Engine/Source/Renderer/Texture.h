#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/ImageDecoder.h"

#include <string>

namespace HachimiEngine
{
    // How a texture is addressed outside the 0..1 range and how it is sampled. These are the
    // renderer's own names rather than the asset layer's, so the renderer keeps no dependency on
    // AssetMeta; the texture cache is the one place that translates between the two vocabularies.
    enum class TextureAddressMode
    {
        Repeat = 0,
        Clamp = 1,
        MirroredRepeat = 2
    };

    enum class TextureSamplingFilter
    {
        Nearest = 0,
        Linear = 1
    };

    struct TextureSpecification
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Channels = 4;
        bool SRGB = true;
        bool GenerateMips = true;
        TextureAddressMode Address = TextureAddressMode::Repeat;
        TextureSamplingFilter Filter = TextureSamplingFilter::Linear;
    };

    // Base class for GPU texture resources.
    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual void Bind(uint32_t slot = 0) const = 0;

        virtual void SetData(void* data, uint32_t size) = 0;
    };

    class Texture2D : public Texture
    {
    public:
        static Ref<Texture2D> Create(const TextureSpecification& specification);
        // Uploads pre-decoded pixels under an explicit specification, which is how texture
        // import settings (colour space, mipmaps, wrap, filter) reach the GPU. Always succeeds
        // for a valid image; the async loading path uses it on the main thread.
        static Ref<Texture2D> Create(const TextureSpecification& specification, const DecodedImage& image);
        // Uploads pre-decoded pixels with default settings.
        static Ref<Texture2D> Create(const DecodedImage& image);
        // Returns nullptr when the file is missing or cannot be decoded, so
        // callers can fall back to an untextured material.
        static Ref<Texture2D> Create(const std::string& path);
    };
}
