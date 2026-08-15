#include "Platform/OpenGL/OpenGLShadowMap.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    OpenGLShadowMap::OpenGLShadowMap(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height)
    {
        Invalidate();
    }

    OpenGLShadowMap::~OpenGLShadowMap()
    {
        glDeleteFramebuffers(1, &m_Framebuffer);
        glDeleteTextures(1, &m_DepthTexture);
    }

    void OpenGLShadowMap::BindForWriting()
    {
        // Shadow passes run nested inside the viewport's scene framebuffer, so the
        // previous binding and viewport must be restored afterwards.
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_PreviousFramebuffer);
        glGetIntegerv(GL_VIEWPORT, m_PreviousViewport.data());

        glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);
        glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLShadowMap::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_PreviousFramebuffer));
        glViewport(
            m_PreviousViewport[0],
            m_PreviousViewport[1],
            m_PreviousViewport[2],
            m_PreviousViewport[3]);
    }

    void OpenGLShadowMap::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (width == m_Width && height == m_Height))
        {
            return;
        }

        m_Width = width;
        m_Height = height;
        Invalidate();
    }

    void OpenGLShadowMap::Invalidate()
    {
        if (m_Framebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_Framebuffer);
            glDeleteTextures(1, &m_DepthTexture);
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthTexture);
        glTextureStorage2D(
            m_DepthTexture,
            1,
            GL_DEPTH_COMPONENT24,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));

        glTextureParameteri(m_DepthTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_DepthTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        constexpr float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTextureParameterfv(m_DepthTexture, GL_TEXTURE_BORDER_COLOR, borderColor);

        glCreateFramebuffers(1, &m_Framebuffer);
        glNamedFramebufferTexture(m_Framebuffer, GL_DEPTH_ATTACHMENT, m_DepthTexture, 0);
        glNamedFramebufferDrawBuffer(m_Framebuffer, GL_NONE);
        glNamedFramebufferReadBuffer(m_Framebuffer, GL_NONE);

        const GLenum status = glCheckNamedFramebufferStatus(m_Framebuffer, GL_FRAMEBUFFER);
        HE_CORE_ASSERT(status == GL_FRAMEBUFFER_COMPLETE);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            HE_CORE_CRITICAL("Shadow map framebuffer is incomplete: status=0x{:X}", status);
        }
    }
}
