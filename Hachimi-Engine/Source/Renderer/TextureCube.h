#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    enum class CubeMapFace : uint32_t
    {
        PositiveX = 0,
        NegativeX = 1,
        PositiveY = 2,
        NegativeY = 3,
        PositiveZ = 4,
        NegativeZ = 5
    };

    // HDR cubemap texture used for skyboxes and image based lighting.
    class TextureCube
    {
    public:
        virtual ~TextureCube() = default;

        virtual void Bind(uint32_t slot = 0) const = 0;

        virtual void SetFaceData(CubeMapFace face, uint32_t mipLevel, const void* data, uint32_t componentCount) = 0;

        virtual uint32_t GetSize() const = 0;
        virtual uint32_t GetMipLevelCount() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        static Ref<TextureCube> Create(uint32_t size, uint32_t mipLevelCount = 1);
    };
}
