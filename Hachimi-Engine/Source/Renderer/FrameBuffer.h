#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    enum class FramebufferColorFormat
    {
        RGBA8 = 0,
        RGBA16F = 1
    };

    struct FramebufferSpecification
    {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        uint32_t Samples = 1;
        FramebufferColorFormat ColorFormat = FramebufferColorFormat::RGBA8;
    };

    // Off-screen render target abstraction.
    class Framebuffer
    {
    public:
        virtual ~Framebuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t GetColorAttachmentRendererID() const = 0;
        virtual const FramebufferSpecification& GetSpecification() const = 0;

        static Ref<Framebuffer> Create(const FramebufferSpecification& specification);
    };
}
