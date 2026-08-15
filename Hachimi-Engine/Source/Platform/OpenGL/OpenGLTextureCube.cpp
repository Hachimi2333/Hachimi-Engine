#include "Platform/OpenGL/OpenGLTextureCube.h"

#include "Core/Assert.h"

#include <glad/gl.h>

#include <algorithm>

namespace HachimiEngine
{
    OpenGLTextureCube::OpenGLTextureCube(uint32_t size, uint32_t mipLevelCount)
        : m_Size(size), m_MipLevelCount(mipLevelCount)
    {
        HE_CORE_ASSERT(size > 0);
        HE_CORE_ASSERT(mipLevelCount > 0);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(
            m_RendererID,
            static_cast<GLsizei>(mipLevelCount),
            GL_RGBA16F,
            static_cast<GLsizei>(size),
            static_cast<GLsizei>(size));

        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, mipLevelCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    OpenGLTextureCube::~OpenGLTextureCube()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTextureCube::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }

    void OpenGLTextureCube::SetFaceData(CubeMapFace face, uint32_t mipLevel, const void* data, uint32_t componentCount)
    {
        HE_CORE_ASSERT(mipLevel < m_MipLevelCount);
        HE_CORE_ASSERT(componentCount == 3 || componentCount == 4);

        const GLenum format = componentCount == 4 ? GL_RGBA : GL_RGB;
        const uint32_t mipSize = std::max(m_Size >> mipLevel, 1u);

        // For cubemap textures the third coordinate selects one face layer.
        glTextureSubImage3D(
            m_RendererID,
            static_cast<GLint>(mipLevel),
            0,
            0,
            static_cast<GLint>(face),
            static_cast<GLsizei>(mipSize),
            static_cast<GLsizei>(mipSize),
            1,
            format,
            GL_FLOAT,
            data);
    }
}
