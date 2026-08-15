#include "Platform/OpenGL/OpenGLVertexArray.h"

#include "Core/Assert.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    OpenGLVertexArray::OpenGLVertexArray()
    {
        glCreateVertexArrays(1, &m_RendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(m_RendererID);
    }

    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
    {
        HE_CORE_ASSERT(!vertexBuffer->GetLayout().GetElements().empty());

        Bind();
        vertexBuffer->Bind();

        const auto& layout = vertexBuffer->GetLayout();
        for (const auto& element : layout)
        {
            switch (element.Type)
            {
                case ShaderDataType::Float:
                case ShaderDataType::Float2:
                case ShaderDataType::Float3:
                case ShaderDataType::Float4:
                {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribPointer(
                        m_VertexBufferIndex,
                        static_cast<GLint>(ShaderDataTypeComponentCount(element.Type)),
                        ShaderDataTypeToOpenGLBaseType(element.Type),
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        static_cast<GLsizei>(layout.GetStride()),
                        reinterpret_cast<const void*>(element.Offset));
                    m_VertexBufferIndex++;
                    break;
                }
                case ShaderDataType::Int:
                case ShaderDataType::Int2:
                case ShaderDataType::Int3:
                case ShaderDataType::Int4:
                case ShaderDataType::Bool:
                {
                    glEnableVertexAttribArray(m_VertexBufferIndex);
                    glVertexAttribIPointer(
                        m_VertexBufferIndex,
                        static_cast<GLint>(ShaderDataTypeComponentCount(element.Type)),
                        ShaderDataTypeToOpenGLBaseType(element.Type),
                        static_cast<GLsizei>(layout.GetStride()),
                        reinterpret_cast<const void*>(element.Offset));
                    m_VertexBufferIndex++;
                    break;
                }
                case ShaderDataType::None:
                    break;
            }
        }

        m_VertexBuffers.push_back(vertexBuffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
    {
        Bind();
        indexBuffer->Bind();
        m_IndexBuffer = indexBuffer;
    }
}
