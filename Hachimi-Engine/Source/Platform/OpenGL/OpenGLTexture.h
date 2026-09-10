#pragma once

#include "Renderer/ImageDecoder.h"
#include "Renderer/Texture.h"

#include <string>

namespace HachimiEngine
{
    class OpenGLTexture2D final : public Texture2D
    {
    public:
        explicit OpenGLTexture2D(const TextureSpecification& specification);
        explicit OpenGLTexture2D(const std::string& path);
        // Uploads pixels that were already decoded, normally on a worker thread.
        explicit OpenGLTexture2D(const DecodedImage& image);
        ~OpenGLTexture2D() override;

        uint32_t GetWidth() const override { return m_Specification.Width; }
        uint32_t GetHeight() const override { return m_Specification.Height; }
        uint32_t GetRendererID() const override { return m_RendererID; }

        void Bind(uint32_t slot = 0) const override;

        void SetData(void* data, uint32_t size) override;

    private:
        void InitializeFromImage(const DecodedImage& image);

        TextureSpecification m_Specification;
        uint32_t m_RendererID = 0;
    };
}
