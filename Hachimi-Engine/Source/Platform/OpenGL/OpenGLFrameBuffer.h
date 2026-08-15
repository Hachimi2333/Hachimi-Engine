#pragma once

#include "Renderer/FrameBuffer.h"

namespace HachimiEngine
{
    class OpenGLFramebuffer final : public Framebuffer
    {
    public:
        explicit OpenGLFramebuffer(const FramebufferSpecification& specification);
        ~OpenGLFramebuffer() override;

        void Bind() override;
        void Unbind() override;

        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetColorAttachmentRendererID() const override { return m_ColorAttachment; }
        const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

    private:
        void Invalidate();

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_DepthAttachment = 0;
        FramebufferSpecification m_Specification;
    };
}
