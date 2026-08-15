#include "Platform/OpenGL/OpenGLTexture.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <glad/gl.h>
#include <stb_image.h>

namespace HachimiEngine
{
    OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
        : m_Specification(specification)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGBA8, static_cast<GLsizei>(specification.Width), static_cast<GLsizei>(specification.Height));

        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGBA8, width, height);

        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

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
    }
}
