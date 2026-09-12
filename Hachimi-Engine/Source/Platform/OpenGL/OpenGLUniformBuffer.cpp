#include "Platform/OpenGL/OpenGLUniformBuffer.h"

#include "Core/Assert.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t bindingPoint)
        : m_BindingPoint(bindingPoint)
    {
        HE_CORE_ASSERT(size > 0);

        glCreateBuffers(1, &m_RendererID);
        // GL_DYNAMIC_STORAGE_BIT is what lets SetData update the store after creation.
        glNamedBufferStorage(m_RendererID, size, nullptr, GL_DYNAMIC_STORAGE_BIT);
    }

    OpenGLUniformBuffer::~OpenGLUniformBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void OpenGLUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
    {
        glNamedBufferSubData(m_RendererID, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    }

    void OpenGLUniformBuffer::Bind() const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingPoint, m_RendererID);
    }
}
