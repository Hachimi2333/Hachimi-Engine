#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    class TextureCube;

    // GPU textures uploaded from the build-time baked sky environment.
    class EnvironmentMap
    {
    public:
        EnvironmentMap();
        ~EnvironmentMap() = default;

        void BindSkybox(uint32_t slot) const;
        void BindIrradiance(uint32_t slot) const;
        void BindPrefiltered(uint32_t slot) const;

        // False when the cubemaps could not be generated, in which case the renderer
        // skips the skybox and image-based lighting instead of binding nothing.
        bool HasContent() const;

        uint32_t GetSkyboxRendererID() const;
        uint32_t GetIrradianceRendererID() const;
        uint32_t GetPrefilteredRendererID() const;

        uint32_t GetPrefilteredMipLevelCount() const;

    private:
        Ref<TextureCube> m_Skybox;
        Ref<TextureCube> m_Irradiance;
        Ref<TextureCube> m_Prefiltered;
    };
}
