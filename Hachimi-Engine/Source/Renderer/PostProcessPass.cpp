#include "Renderer/PostProcessPass.h"

#include "Core/Assert.h"
#include "Renderer/Shader.h"

#include <glad/gl.h>

namespace HachimiEngine
{
    Ref<Shader> PostProcessPass::s_Shader;
    uint32_t PostProcessPass::s_VertexArray = 0;
    float PostProcessPass::s_Exposure = 1.0f;

    void PostProcessPass::Init()
    {
        s_Shader = Shader::CreateEngineShader("PostProcess.glsl");
        glCreateVertexArrays(1, &s_VertexArray);
    }

    void PostProcessPass::Shutdown()
    {
        if (s_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &s_VertexArray);
            s_VertexArray = 0;
        }

        s_Shader.reset();
    }

    void PostProcessPass::Render(uint32_t inputTexture)
    {
        HE_CORE_ASSERT(s_Shader != nullptr);
        HE_CORE_ASSERT(s_VertexArray != 0);

        s_Shader->Bind();
        s_Shader->SetInt("u_SceneTexture", 0);
        s_Shader->SetFloat("u_Exposure", s_Exposure);

        glBindTextureUnit(0, inputTexture);
        glBindVertexArray(s_VertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}
