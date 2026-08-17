#include "Renderer/EnvironmentMap.h"

#include "Core/Assert.h"
#include "Renderer/TextureCube.h"
#include "Math/Math.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        constexpr uint32_t IrradianceResolution = 32;
        constexpr uint32_t IrradianceSampleCount = 512;
        constexpr uint32_t PrefilteredMipLevelCount = 4;
        constexpr uint32_t PrefilteredSampleCount = 128;

        constexpr float SunDiscExponent = 800.0f;
        const Math::Vec3 SunDirection = Math::Normalize(Math::Vec3(-0.55f, 0.42f, -0.72f));
        const Math::Vec3 SunDiscColor(60.0f, 48.0f, 34.0f);
        const Math::Vec3 SunGlowColor(1.2f, 0.9f, 0.6f);

        float RadicalInverseVdC(uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

        Math::Vec2 Hammersley(uint32_t index, uint32_t sampleCount)
        {
            return { static_cast<float>(index) / static_cast<float>(sampleCount), RadicalInverseVdC(index) };
        }

        Math::Vec3 ImportanceSampleGGX(Math::Vec2 xi, float roughness, Math::Vec3 normal)
        {
            const float roughness2 = roughness * roughness;
            const float phi = Math::TwoPi<float>() * xi.x;
            const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (roughness2 * roughness2 - 1.0f) * xi.y));
            const float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));

            const Math::Vec3 upDirection = std::abs(normal.y) < 0.999f
                ? Math::Vec3(0.0f, 1.0f, 0.0f)
                : Math::Vec3(1.0f, 0.0f, 0.0f);
            const Math::Vec3 tangent = Math::Normalize(Math::Cross(upDirection, normal));
            const Math::Vec3 bitangent = Math::Cross(normal, tangent);

            const Math::Vec3 sampleDirection = tangent * (sinTheta * std::cos(phi))
                + bitangent * (sinTheta * std::sin(phi))
                + normal * cosTheta;
            return Math::Normalize(sampleDirection);
        }

        float DistributionGGX(float normalDotHalf, float roughness)
        {
            const float roughness2 = roughness * roughness;
            const float roughness4 = roughness2 * roughness2;
            const float denominator = normalDotHalf * normalDotHalf * (roughness4 - 1.0f) + 1.0f;
            return roughness4 / (Math::Pi<float>() * denominator * denominator);
        }

        // Sky gradient plus the broad sun glow. The narrow sun disc is handled
        // separately by the integrators below because finite-sample convolution
        // cannot resolve such a small lobe without producing bright speckles.
        Math::Vec3 EvaluateSkyBase(Math::Vec3 direction)
        {
            const float height = Math::Clamp(direction.y * 0.5f + 0.5f, 0.0f, 1.0f);

            const Math::Vec3 horizonColor(1.0f, 0.72f, 0.48f);
            const Math::Vec3 zenithColor(0.06f, 0.16f, 0.42f);
            const Math::Vec3 groundColor(0.025f, 0.025f, 0.035f);

            Math::Vec3 color = Math::Mix(horizonColor, zenithColor, std::pow(height, 0.65f));
            if (direction.y < 0.0f)
            {
                color = Math::Mix(groundColor, horizonColor, 1.0f + direction.y);
            }

            const float sunGlow = std::pow(std::max(Math::Dot(direction, SunDirection), 0.0f), 16.0f);
            return color + SunGlowColor * sunGlow;
        }
    }

    EnvironmentMap::EnvironmentMap(uint32_t resolution)
        : m_Resolution(std::max(resolution, 16u))
    {
        m_Skybox = TextureCube::Create(m_Resolution, 1);
        m_Irradiance = TextureCube::Create(IrradianceResolution, 1);
        m_Prefiltered = TextureCube::Create(m_Resolution, PrefilteredMipLevelCount);

        GenerateSkybox();
        GenerateIrradianceMap();
        GeneratePrefilteredMap();
    }

    void EnvironmentMap::BindSkybox(uint32_t slot) const
    {
        m_Skybox->Bind(slot);
    }

    void EnvironmentMap::BindIrradiance(uint32_t slot) const
    {
        m_Irradiance->Bind(slot);
    }

    void EnvironmentMap::BindPrefiltered(uint32_t slot) const
    {
        m_Prefiltered->Bind(slot);
    }

    uint32_t EnvironmentMap::GetSkyboxRendererID() const
    {
        return m_Skybox->GetRendererID();
    }

    uint32_t EnvironmentMap::GetIrradianceRendererID() const
    {
        return m_Irradiance->GetRendererID();
    }

    uint32_t EnvironmentMap::GetPrefilteredRendererID() const
    {
        return m_Prefiltered->GetRendererID();
    }

    uint32_t EnvironmentMap::GetPrefilteredMipLevelCount() const
    {
        return m_Prefiltered->GetMipLevelCount();
    }

    Math::Vec3 EnvironmentMap::EvaluateSky(Math::Vec3 direction)
    {
        Math::Vec3 color = EvaluateSkyBase(direction);

        const float sunDisc = std::pow(std::max(Math::Dot(direction, SunDirection), 0.0f), SunDiscExponent);
        return color + SunDiscColor * sunDisc;
    }

    Math::Vec3 EnvironmentMap::CubeMapFaceDirection(uint32_t faceIndex, float u, float v)
    {
        switch (static_cast<CubeMapFace>(faceIndex))
        {
            case CubeMapFace::PositiveX: return Math::Normalize(Math::Vec3( 1.0f, -v, -u));
            case CubeMapFace::NegativeX: return Math::Normalize(Math::Vec3(-1.0f, -v,  u));
            case CubeMapFace::PositiveY: return Math::Normalize(Math::Vec3( u,   1.0f,  v));
            case CubeMapFace::NegativeY: return Math::Normalize(Math::Vec3( u,  -1.0f, -v));
            case CubeMapFace::PositiveZ: return Math::Normalize(Math::Vec3( u,  -v,  1.0f));
            case CubeMapFace::NegativeZ: return Math::Normalize(Math::Vec3(-u,  -v, -1.0f));
        }

        HE_CORE_ASSERT(false);
        return Math::Vec3(0.0f, 1.0f, 0.0f);
    }

    void EnvironmentMap::GenerateSkybox()
    {
        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            std::vector<float> faceData(static_cast<size_t>(m_Resolution) * m_Resolution * 4);
            for (uint32_t y = 0; y < m_Resolution; ++y)
            {
                for (uint32_t x = 0; x < m_Resolution; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(m_Resolution) * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(m_Resolution) * 2.0f - 1.0f;
                    const Math::Vec3 direction = CubeMapFaceDirection(faceIndex, u, v);
                    const Math::Vec3 color = EvaluateSky(direction);

                    float* texel = &faceData[(static_cast<size_t>(y) * m_Resolution + x) * 4];
                    texel[0] = color.r;
                    texel[1] = color.g;
                    texel[2] = color.b;
                    texel[3] = 1.0f;
                }
            }

            m_Skybox->SetFaceData(static_cast<CubeMapFace>(faceIndex), 0, faceData.data(), 4);
        }
    }

    void EnvironmentMap::GenerateIrradianceMap()
    {
        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            std::vector<float> faceData(static_cast<size_t>(IrradianceResolution) * IrradianceResolution * 4);
            for (uint32_t y = 0; y < IrradianceResolution; ++y)
            {
                for (uint32_t x = 0; x < IrradianceResolution; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / IrradianceResolution * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + 0.5f) / IrradianceResolution * 2.0f - 1.0f;
                    const Math::Vec3 normal = CubeMapFaceDirection(faceIndex, u, v);

                    const Math::Vec3 upDirection = std::abs(normal.y) < 0.999f
                        ? Math::Vec3(0.0f, 1.0f, 0.0f)
                        : Math::Vec3(1.0f, 0.0f, 0.0f);
                    const Math::Vec3 tangent = Math::Normalize(Math::Cross(upDirection, normal));
                    const Math::Vec3 bitangent = Math::Cross(normal, tangent);

                    Math::Vec3 irradiance(0.0f);
                    for (uint32_t sampleIndex = 0; sampleIndex < IrradianceSampleCount; ++sampleIndex)
                    {
                        const Math::Vec2 xi = Hammersley(sampleIndex, IrradianceSampleCount);
                        const float phi = Math::TwoPi<float>() * xi.x;
                        const float cosTheta = std::sqrt(xi.y);
                        const float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));

                        Math::Vec3 sampleDirection = tangent * (sinTheta * std::cos(phi))
                            + bitangent * (sinTheta * std::sin(phi))
                            + normal * cosTheta;
                        sampleDirection = Math::Normalize(sampleDirection);

                        irradiance += EvaluateSkyBase(sampleDirection);
                    }

                    irradiance /= static_cast<float>(IrradianceSampleCount);

                    // The sun disc is a narrow, high-energy lobe. Monte Carlo
                    // sampling misses it on some texels and produces the bright
                    // speckles on diffuse materials, so add its cosine-weighted
                    // irradiance contribution analytically instead.
                    const float sunMass = Math::TwoPi<float>() / (SunDiscExponent + 1.0f);
                    const float normalDotSun = std::max(Math::Dot(normal, SunDirection), 0.0f);
                    irradiance += SunDiscColor * (sunMass / Math::Pi<float>()) * normalDotSun;

                    float* texel = &faceData[(static_cast<size_t>(y) * IrradianceResolution + x) * 4];
                    texel[0] = irradiance.r;
                    texel[1] = irradiance.g;
                    texel[2] = irradiance.b;
                    texel[3] = 1.0f;
                }
            }

            m_Irradiance->SetFaceData(static_cast<CubeMapFace>(faceIndex), 0, faceData.data(), 4);
        }
    }

    void EnvironmentMap::GeneratePrefilteredMap()
    {
        for (uint32_t mipLevel = 0; mipLevel < PrefilteredMipLevelCount; ++mipLevel)
        {
            const float roughness = static_cast<float>(mipLevel) / static_cast<float>(PrefilteredMipLevelCount - 1);
            const uint32_t mipResolution = std::max(m_Resolution >> mipLevel, 1u);

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                std::vector<float> faceData(static_cast<size_t>(mipResolution) * mipResolution * 4);
                for (uint32_t y = 0; y < mipResolution; ++y)
                {
                    for (uint32_t x = 0; x < mipResolution; ++x)
                    {
                        const float u = (static_cast<float>(x) + 0.5f) / mipResolution * 2.0f - 1.0f;
                        const float v = (static_cast<float>(y) + 0.5f) / mipResolution * 2.0f - 1.0f;
                        const Math::Vec3 normal = CubeMapFaceDirection(faceIndex, u, v);
                        const Math::Vec3 viewDirection = normal;

                        Math::Vec3 prefilteredColor(0.0f);
                        if (mipLevel == 0)
                        {
                            // With zero roughness every GGX sample collapses to the
                            // same direction, so evaluate the mirror image directly.
                            prefilteredColor = EvaluateSky(normal);
                        }
                        else
                        {
                            float totalWeight = 0.0f;
                            for (uint32_t sampleIndex = 0; sampleIndex < PrefilteredSampleCount; ++sampleIndex)
                            {
                                const Math::Vec2 xi = Hammersley(sampleIndex, PrefilteredSampleCount);
                                const Math::Vec3 halfVector = ImportanceSampleGGX(xi, roughness, normal);
                                const Math::Vec3 lightDirection = Math::Normalize(2.0f * Math::Dot(viewDirection, halfVector) * halfVector - viewDirection);

                                const float normalDotLight = Math::Dot(normal, lightDirection);
                                if (normalDotLight > 0.0f)
                                {
                                    prefilteredColor += EvaluateSkyBase(lightDirection) * normalDotLight;
                                    totalWeight += normalDotLight;
                                }
                            }

                            if (totalWeight > 0.0f)
                            {
                                prefilteredColor /= totalWeight;

                                // Add the narrow sun disc analytically. The GGX
                                // samples cover the broad sky and glow; finite
                                // sampling of the sun lobe itself leaves bright
                                // speckles in the rough reflection.
                                const float normalDotSun = std::max(Math::Dot(normal, SunDirection), 0.0f);
                                if (normalDotSun > 0.0f)
                                {
                                    const Math::Vec3 sunHalfVector = Math::Normalize(normal + SunDirection);
                                    const float normalDotHalf = std::max(Math::Dot(normal, sunHalfVector), 0.0f);
                                    if (normalDotHalf > 0.0f)
                                    {
                                        const float distribution = DistributionGGX(normalDotHalf, roughness);
                                        const float sunMass = Math::TwoPi<float>() / (SunDiscExponent + 1.0f);
                                        const Math::Vec3 sunContribution = SunDiscColor * (normalDotSun * distribution * sunMass / 4.0f);

                                        // totalWeight is the sum over N samples;
                                        // rescale the continuous sun integral to
                                        // the same estimator normalization.
                                        prefilteredColor += sunContribution * static_cast<float>(PrefilteredSampleCount) / totalWeight;
                                    }
                                }
                            }
                        }

                        float* texel = &faceData[(static_cast<size_t>(y) * mipResolution + x) * 4];
                        texel[0] = prefilteredColor.r;
                        texel[1] = prefilteredColor.g;
                        texel[2] = prefilteredColor.b;
                        texel[3] = 1.0f;
                    }
                }

                m_Prefiltered->SetFaceData(static_cast<CubeMapFace>(faceIndex), mipLevel, faceData.data(), 4);
            }
        }
    }
}
