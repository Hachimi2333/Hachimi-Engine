#pragma once

#include "Renderer/UniformBuffer.h"

namespace HachimiEngine
{
    class OpenGLUniformBuffer final : public UniformBuffer
    {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t bindingPoint);
        ~OpenGLUniformBuffer() override;

        OpenGLUniformBuffer(const OpenGLUniformBuffer&) = delete;
        OpenGLUniformBuffer& operator=(const OpenGLUniformBuffer&) = delete;

        void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
        void Bind() const override;

        uint32_t GetBindingPoint() const override { return m_BindingPoint; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_BindingPoint = 0;
    };
}
