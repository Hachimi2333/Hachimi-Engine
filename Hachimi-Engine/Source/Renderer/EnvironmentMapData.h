#pragma once

#include "Math/Math.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace HachimiEngine
{
    // CPU-only RGBA32F cubemap faces. Generation is used by the baker, never by startup.
    struct EnvironmentMapData
    {
        static constexpr uint32_t IrradianceSize = 32;
        static constexpr uint32_t MipCount = 4;
        static constexpr const char* ResourcePath = "Environment/Default.henv";
        using Faces = std::array<std::vector<float>, 6>;

        uint32_t Resolution = 128;
        Faces Skybox;
        Faces Irradiance;
        std::array<Faces, MipCount> Prefiltered;

        static EnvironmentMapData Generate(uint32_t resolution = 128);
        std::vector<uint8_t> Serialize() const;
        static bool Deserialize(std::span<const uint8_t> bytes, EnvironmentMapData& out, std::string& error);

    private:
        void GenerateSkybox();
        void GenerateIrradianceMap();
        void GeneratePrefilteredMap();
        static Math::Vec3 EvaluateSky(Math::Vec3 direction);
        static Math::Vec3 CubeMapFaceDirection(uint32_t faceIndex, float u, float v);
    };
}
