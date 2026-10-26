#include <doctest/doctest.h>

#include "Renderer/EnvironmentMapData.h"
#include "Utils/PlatformUtils.h"
#include "Utils/VirtualFileSystem.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace HachimiEngine;

namespace
{
    const EnvironmentMapData& SmallEnvironment()
    {
        static const auto data = EnvironmentMapData::Generate(16);
        return data;
    }
}

TEST_SUITE("Renderer")
{
    TEST_CASE("environment baking preserves the mirror sky and finite HDR illumination")
    {
        const auto& data = SmallEnvironment();
        for (size_t face = 0; face < 6; ++face)
        {
            CHECK(data.Skybox[face].size() == 16 * 16 * 4);
            CHECK(data.Irradiance[face].size() == 32 * 32 * 4);
            CHECK(data.Prefiltered[0][face] == data.Skybox[face]);
            for (uint32_t mip = 0; mip < EnvironmentMapData::MipCount; ++mip)
            {
                const auto& pixels = data.Prefiltered[mip][face];
                CHECK(pixels.size() == static_cast<size_t>(16 >> mip) * (16 >> mip) * 4);
                for (size_t texel = 0; texel < pixels.size(); texel += 4)
                {
                    REQUIRE(std::isfinite(pixels[texel]));
                    REQUIRE(std::isfinite(pixels[texel + 1]));
                    REQUIRE(std::isfinite(pixels[texel + 2]));
                    REQUIRE(pixels[texel] >= 0.0f);
                    REQUIRE(pixels[texel + 3] == 1.0f);
                }
            }
        }
        CHECK_THROWS_AS(EnvironmentMapData::Generate(15), std::invalid_argument);
        CHECK_THROWS_AS(EnvironmentMapData::Generate(17), std::invalid_argument);
    }

    TEST_CASE("environment serialization is lossless and rejects invalid resources without replacing output")
    {
        const auto& data = SmallEnvironment();
        const auto bytes = data.Serialize();
        EnvironmentMapData restored;
        std::string error;
        REQUIRE(EnvironmentMapData::Deserialize(bytes, restored, error));
        CHECK(restored.Serialize() == bytes);

        auto invalid = bytes;
        invalid[0] ^= 0xff;
        CHECK_FALSE(EnvironmentMapData::Deserialize(invalid, restored, error));
        CHECK_FALSE(error.empty());
        CHECK(restored.Serialize() == bytes);
        CHECK_FALSE(EnvironmentMapData::Deserialize(std::span(bytes).first(19), restored, error));
        CHECK_FALSE(EnvironmentMapData::Deserialize(std::span(bytes).first(bytes.size() - 1), restored, error));
        invalid = bytes;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::memcpy(invalid.data() + 20, &nan, sizeof(nan));
        CHECK_FALSE(EnvironmentMapData::Deserialize(invalid, restored, error));
    }

    TEST_CASE("the build ships a full resolution baked environment readable without OpenGL")
    {
        std::vector<uint8_t> bytes;
        REQUIRE(VirtualFileSystem::ReadBinaryFile(PlatformUtils::GetExecutableDirectory() / EnvironmentMapData::ResourcePath, bytes));
        EnvironmentMapData data;
        std::string error;
        REQUIRE(EnvironmentMapData::Deserialize(bytes, data, error));
        CHECK(data.Resolution == 128);
        CHECK(data.Skybox[0].size() == 128 * 128 * 4);
    }
}
