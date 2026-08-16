#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include "Renderer/VertexArray.h"
#include "Math/Math.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    void OpenGLRendererAPI::Init()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }

    void OpenGLRendererAPI::SetClearColor(const Math::Vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void OpenGLRendererAPI::SetDepthTest(bool enabled)
    {
        if (enabled)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }

    void OpenGLRendererAPI::SetBlend(bool enabled)
    {
        if (enabled)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    void OpenGLRendererAPI::SetLineWidth(float width)
    {
        glLineWidth(width);
    }

    void OpenGLRendererAPI::SetPolygonOffset(bool enabled, float factor, float units)
    {
        if (enabled)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(factor, units);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, DrawMode drawMode)
    {
        vertexArray->Bind();

        const uint32_t count = indexCount > 0 ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        const GLenum primitiveType = drawMode == DrawMode::Lines ? GL_LINES : GL_TRIANGLES;
        glDrawElements(primitiveType, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
    }
}
