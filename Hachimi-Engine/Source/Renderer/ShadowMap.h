#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    // Depth-only render target used by shadow-casting lights.
    class ShadowMap
    {
    public:
        virtual ~ShadowMap() = default;

        virtual void BindForWriting() = 0;
        virtual void Unbind() = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetDepthTextureRendererID() const = 0;

        static Ref<ShadowMap> Create(uint32_t width, uint32_t height);
    };
}
