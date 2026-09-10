#include "Platform/OpenGL/OpenGLTexture.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Renderer/ImageDecoder.h"
#include "Utils/FileMapping.h"
#include "Utils/VirtualFileSystem.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        uint32_t CalculateMipLevelCount(uint32_t width, uint32_t height)
        {
            const uint32_t largestDimension = std::max(width, height);
            return 1 + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(largestDimension))));
        }

        void ApplyTextureParameters(uint32_t rendererID, bool useMipmaps)
        {
            glTextureParameteri(rendererID, GL_TEXTURE_MIN_FILTER, useMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTextureParameteri(rendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

            // Anisotropic filtering reduces blur and shimmer on grazing surfaces.
            float maxAnisotropy = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
            glTextureParameterf(rendererID, GL_TEXTURE_MAX_ANISOTROPY, std::min(maxAnisotropy, 8.0f));
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
        : m_Specification(specification)
    {
        const uint32_t mipLevelCount = specification.GenerateMips
            ? CalculateMipLevelCount(specification.Width, specification.Height)
            : 1;
        const GLenum internalFormat = specification.SRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(
            m_RendererID,
            static_cast<GLsizei>(mipLevelCount),
            internalFormat,
            static_cast<GLsizei>(specification.Width),
            static_cast<GLsizei>(specification.Height));

        ApplyTextureParameters(m_RendererID, specification.GenerateMips);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
    {
        // Stored textures are mapped zero copy straight out of the game package;
        // compressed ones are inflated once. Either way decoding is CPU only.
        FileMapping encoded;
        if (!VirtualFileSystem::MapFile(path, encoded))
        {
            HE_CORE_ERROR("Failed to read texture file: {}", path);
            HE_CORE_ASSERT(false);
            return;
        }

        DecodedImage image;
        if (!ImageDecoder::DecodeFromMemory(encoded.Data(), encoded.Size(), image))
        {
            HE_CORE_ERROR("Failed to decode texture file: {}", path);
            HE_CORE_ASSERT(false);
            return;
        }

        InitializeFromImage(image);
    }

    OpenGLTexture2D::OpenGLTexture2D(const DecodedImage& image)
    {
        if (!image.IsValid())
        {
            HE_CORE_ERROR("Cannot create a texture from an empty decoded image");
            HE_CORE_ASSERT(false);
            return;
        }

        InitializeFromImage(image);
    }

    void OpenGLTexture2D::InitializeFromImage(const DecodedImage& image)
    {
        m_Specification.Width = image.Width;
        m_Specification.Height = image.Height;
        m_Specification.Channels = 4;
        m_Specification.SRGB = true;
        m_Specification.GenerateMips = true;

        const uint32_t mipLevelCount = CalculateMipLevelCount(m_Specification.Width, m_Specification.Height);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(
            m_RendererID,
            static_cast<GLsizei>(mipLevelCount),
            GL_SRGB8_ALPHA8,
            static_cast<GLsizei>(image.Width),
            static_cast<GLsizei>(image.Height));

        ApplyTextureParameters(m_RendererID, true);

        SetData(const_cast<uint8_t*>(image.Pixels.data()), static_cast<uint32_t>(image.Pixels.size()));
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }

    void OpenGLTexture2D::SetData(void* data, uint32_t size)
    {
        HE_CORE_ASSERT(size == m_Specification.Width * m_Specification.Height * 4);
        glTextureSubImage2D(
            m_RendererID,
            0,
            0,
            0,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            data);

        if (m_Specification.GenerateMips)
        {
            glGenerateTextureMipmap(m_RendererID);
        }
    }
}
