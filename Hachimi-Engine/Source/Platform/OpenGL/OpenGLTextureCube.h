#pragma once

#include "Renderer/TextureCube.h"

namespace HachimiEngine
{
    class OpenGLTextureCube final : public TextureCube
    {
    public:
        OpenGLTextureCube(uint32_t size, uint32_t mipLevelCount);
        ~OpenGLTextureCube() override;

        void Bind(uint32_t slot = 0) const override;

        void SetFaceData(CubeMapFace face, uint32_t mipLevel, const void* data, uint32_t componentCount) override;

        uint32_t GetSize() const override { return m_Size; }
        uint32_t GetMipLevelCount() const override { return m_MipLevelCount; }
        uint32_t GetRendererID() const override { return m_RendererID; }

    private:
        uint32_t m_Size = 1;
        uint32_t m_MipLevelCount = 1;
        uint32_t m_RendererID = 0;
    };
}
