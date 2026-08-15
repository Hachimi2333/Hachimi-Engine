#pragma once

#include "Renderer/ShadowMap.h"

namespace HachimiEngine
{
    class OpenGLShadowMap final : public ShadowMap
    {
    public:
        OpenGLShadowMap(uint32_t width, uint32_t height);
        ~OpenGLShadowMap() override;

        void BindForWriting() override;
        void Unbind() override;

        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }
        uint32_t GetDepthTextureRendererID() const override { return m_DepthTexture; }

    private:
        void Invalidate();

    private:
        uint32_t m_Width = 1;
        uint32_t m_Height = 1;
        uint32_t m_Framebuffer = 0;
        uint32_t m_DepthTexture = 0;
    };
}
