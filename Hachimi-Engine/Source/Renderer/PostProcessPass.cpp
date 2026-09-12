#include "Renderer/PostProcessPass.h"

#include "Core/Assert.h"
#include "Renderer/Shader.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    PostProcessPass::PostProcessPass()
    {
        m_Shader = Shader::CreateEngineShader("PostProcess.glsl");
        glCreateVertexArrays(1, &m_VertexArray);
    }

    PostProcessPass::~PostProcessPass()
    {
        if (m_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &m_VertexArray);
            m_VertexArray = 0;
        }

        m_Shader.reset();
    }

    void PostProcessPass::Render(uint32_t inputTexture, float exposure)
    {
        HE_CORE_ASSERT(m_Shader != nullptr);
        HE_CORE_ASSERT(m_VertexArray != 0);

        m_Shader->Bind();
        m_Shader->SetInt("u_SceneTexture", 0);
        m_Shader->SetFloat("u_Exposure", exposure);

        glBindTextureUnit(0, inputTexture);
        glBindVertexArray(m_VertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}
