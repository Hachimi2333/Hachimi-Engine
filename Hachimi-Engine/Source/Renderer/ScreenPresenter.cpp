#include "Renderer/ScreenPresenter.h"

#include "Core/Assert.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shader.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    ScreenPresenter::ScreenPresenter()
    {
        m_Shader = Shader::CreateEngineShader("Present.glsl");
        glCreateVertexArrays(1, &m_VertexArray);
    }

    ScreenPresenter::~ScreenPresenter()
    {
        if (m_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &m_VertexArray);
            m_VertexArray = 0;
        }

        m_Shader.reset();
    }

    void ScreenPresenter::Present(uint32_t sourceTexture, uint32_t width, uint32_t height) const
    {
        HE_CORE_ASSERT(m_Shader != nullptr);
        HE_CORE_ASSERT(m_VertexArray != 0);
        HE_CORE_ASSERT(sourceTexture != 0);

        // The triangle covers normalized device coordinates, so the viewport is the rectangle
        // the image ends up filling.
        RenderCommand::SetViewport(0, 0, width, height);

        m_Shader->Bind();
        m_Shader->SetInt("u_SourceTexture", 0);
        Renderer::BindTextureUnit(0, sourceTexture);

        glBindVertexArray(m_VertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}
