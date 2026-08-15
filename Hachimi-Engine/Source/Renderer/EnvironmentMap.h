#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    class TextureCube;

    // Procedural sky environment with CPU-generated diffuse and specular IBL cubemaps.
    class EnvironmentMap
    {
    public:
        explicit EnvironmentMap(uint32_t resolution = 128);
        ~EnvironmentMap() = default;

        void BindSkybox(uint32_t slot) const;
        void BindIrradiance(uint32_t slot) const;
        void BindPrefiltered(uint32_t slot) const;

        uint32_t GetSkyboxRendererID() const;
        uint32_t GetIrradianceRendererID() const;
        uint32_t GetPrefilteredRendererID() const;

        uint32_t GetPrefilteredMipLevelCount() const;

    private:
        void GenerateSkybox();
        void GenerateIrradianceMap();
        void GeneratePrefilteredMap();

        static glm::vec3 EvaluateSky(glm::vec3 direction);
        static glm::vec3 CubeMapFaceDirection(uint32_t faceIndex, float u, float v);

    private:
        uint32_t m_Resolution = 128;
        Ref<TextureCube> m_Skybox;
        Ref<TextureCube> m_Irradiance;
        Ref<TextureCube> m_Prefiltered;
    };
}
