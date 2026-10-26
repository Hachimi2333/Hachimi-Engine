#include "Renderer/EnvironmentMap.h"
#include "Renderer/EnvironmentMapData.h"
#include "Renderer/TextureCube.h"
#include "Core/StartupTimer.h"
#include "Utils/VirtualFileSystem.h"

#include <stdexcept>

namespace HachimiEngine
{
    EnvironmentMap::EnvironmentMap()
    {
        StartupTimer timer("Environment load and upload");
        std::vector<uint8_t> bytes;
        EnvironmentMapData data;
        std::string error = EnvironmentMapData::ResourcePath;
        if (!VirtualFileSystem::ReadBinaryFile(VirtualFileSystem::GetDataRoot() / EnvironmentMapData::ResourcePath, bytes)
            || !EnvironmentMapData::Deserialize(bytes, data, error))
            throw std::runtime_error("Cannot load baked environment resource: " + error);

        m_Skybox = TextureCube::Create(data.Resolution, 1);
        m_Irradiance = TextureCube::Create(EnvironmentMapData::IrradianceSize, 1);
        m_Prefiltered = TextureCube::Create(data.Resolution, EnvironmentMapData::MipCount);
        for (uint32_t face = 0; face < 6; ++face)
        {
            m_Skybox->SetFaceData(static_cast<CubeMapFace>(face), 0, data.Skybox[face].data(), 4);
            m_Irradiance->SetFaceData(static_cast<CubeMapFace>(face), 0, data.Irradiance[face].data(), 4);
            for (uint32_t mip = 0; mip < EnvironmentMapData::MipCount; ++mip)
                m_Prefiltered->SetFaceData(static_cast<CubeMapFace>(face), mip, data.Prefiltered[mip][face].data(), 4);
        }
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

    bool EnvironmentMap::HasContent() const
    {
        return m_Skybox != nullptr && m_Irradiance != nullptr && m_Prefiltered != nullptr;
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

}
