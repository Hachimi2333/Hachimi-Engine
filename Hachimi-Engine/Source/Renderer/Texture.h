#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <string>

namespace HachimiEngine
{
    struct TextureSpecification
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Channels = 4;
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
        static Ref<Texture2D> Create(const std::string& path);
    };
}
