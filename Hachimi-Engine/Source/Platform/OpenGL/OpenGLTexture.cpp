#include "Platform/OpenGL/OpenGLTexture.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <glad/gl.h>
#include <stb_image.h>

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
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(1);

        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        HE_CORE_ASSERT(data != nullptr);

        m_Specification.Width = static_cast<uint32_t>(width);
        m_Specification.Height = static_cast<uint32_t>(height);
        m_Specification.Channels = 4;
        m_Specification.SRGB = true;
        m_Specification.GenerateMips = true;

        const uint32_t mipLevelCount = CalculateMipLevelCount(m_Specification.Width, m_Specification.Height);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(
            m_RendererID,
            static_cast<GLsizei>(mipLevelCount),
            GL_SRGB8_ALPHA8,
            width,
            height);

        ApplyTextureParameters(m_RendererID, true);

        SetData(data, static_cast<uint32_t>(width * height * 4));

        stbi_image_free(data);
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
