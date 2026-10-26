#include "Renderer/EnvironmentMapData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace HachimiEngine
{
    namespace
    {
        static_assert(sizeof(float) == 4);
        constexpr uint32_t Magic = 0x564E4548; // HENV, little endian on the supported x64 target.
        constexpr uint32_t Version = 1;

        size_t FloatCount(uint32_t resolution)
        {
            size_t count = 6ull * (resolution * resolution + 32 * 32) * 4;
            for (uint32_t mip = 0; mip < EnvironmentMapData::MipCount; ++mip)
                count += 6ull * (resolution >> mip) * (resolution >> mip) * 4;
            return count;
        }

        template<typename TData, typename TVisit>
        void VisitFaces(TData& data, TVisit visit)
        {
            for (auto& face : data.Skybox) visit(face, data.Resolution);
            for (auto& face : data.Irradiance) visit(face, EnvironmentMapData::IrradianceSize);
            for (uint32_t mip = 0; mip < EnvironmentMapData::MipCount; ++mip)
                for (auto& face : data.Prefiltered[mip]) visit(face, data.Resolution >> mip);
        }
    }

    std::vector<uint8_t> EnvironmentMapData::Serialize() const
    {
        if (Resolution < 16 || Resolution > 512 || (Resolution & (Resolution - 1)) != 0)
            throw std::invalid_argument("Invalid environment resolution");
        const std::array<uint32_t, 5> header { Magic, Version, Resolution, IrradianceSize, MipCount };
        std::vector<uint8_t> bytes(sizeof(header) + FloatCount(Resolution) * sizeof(float));
        std::memcpy(bytes.data(), header.data(), sizeof(header));
        size_t offset = sizeof(header);
        VisitFaces(*this, [&](const auto& face, uint32_t size)
        {
            if (face.size() != static_cast<size_t>(size) * size * 4
                || !std::all_of(face.begin(), face.end(), [](float value) { return std::isfinite(value); }))
                throw std::invalid_argument("Invalid environment face data");
            std::memcpy(bytes.data() + offset, face.data(), face.size() * sizeof(float));
            offset += face.size() * sizeof(float);
        });
        return bytes;
    }

    bool EnvironmentMapData::Deserialize(std::span<const uint8_t> bytes, EnvironmentMapData& out, std::string& error)
    {
        error.clear();
        std::array<uint32_t, 5> header {};
        if (bytes.size() < sizeof(header)) { error = "Truncated environment header"; return false; }
        std::memcpy(header.data(), bytes.data(), sizeof(header));
        const uint32_t resolution = header[2];
        if (header[0] != Magic || header[1] != Version || header[3] != IrradianceSize || header[4] != MipCount
            || resolution < 16 || resolution > 512 || (resolution & (resolution - 1)) != 0)
        { error = "Unsupported environment format or dimensions"; return false; }
        if (bytes.size() != sizeof(header) + FloatCount(resolution) * sizeof(float))
        { error = "Invalid environment payload size"; return false; }

        EnvironmentMapData data;
        data.Resolution = resolution;
        size_t offset = sizeof(header);
        bool valid = true;
        VisitFaces(data, [&](auto& face, uint32_t size)
        {
            face.resize(static_cast<size_t>(size) * size * 4);
            std::memcpy(face.data(), bytes.data() + offset, face.size() * sizeof(float));
            offset += face.size() * sizeof(float);
            valid = valid && std::all_of(face.begin(), face.end(), [](float value) { return std::isfinite(value); });
        });
        if (!valid) { error = "Non-finite environment texel"; return false; }
        out = std::move(data);
        return true;
    }
}
