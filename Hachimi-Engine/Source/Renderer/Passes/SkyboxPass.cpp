#include "Renderer/Passes/SkyboxPass.h"

#include "Renderer/EnvironmentMap.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    namespace
    {
        constexpr int SkyboxTextureUnit = 0;
    }

    void SkyboxPass::Execute(RenderPassContext& context)
    {
        const RenderView& view = context.View;
        if (!view.Environment.ShowSkybox)
        {
            return;
        }

        EnvironmentMap& environmentMap = context.Renderers.GetEnvironmentMap();
        if (!environmentMap.HasContent())
        {
            return;
        }

        const Ref<Mesh> gpuMesh = context.Renderers.GetMeshes().GetOrCreate(context.Renderers.GetSkyboxMesh());
        if (gpuMesh == nullptr)
        {
            return;
        }

        // The cube is seen from inside, so its outward-facing triangles are the far side:
        // culling has to be off for the sky to be visible at all.
        Renderer::SetCullMode(CullMode::None);
        Renderer::SetDepthTest(false);

        const Ref<Shader> shader = context.Renderers.GetSkyboxShader();
        shader->Bind();

        // Rotation only: the sky is infinitely far away, so translation must not apply.
        const Math::Mat4 skyViewProjection = view.Projection * Math::Mat4(Math::Mat3(view.View));
        shader->SetMat4("u_ViewProjection", skyViewProjection);
        shader->SetInt("u_SkyboxTexture", SkyboxTextureUnit);
        shader->SetFloat("u_SkyboxIntensity", view.Environment.EnvironmentIntensity);
        environmentMap.BindSkybox(SkyboxTextureUnit);

        Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Triangles);

        Renderer::SetDepthTest(true);
        Renderer::SetCullMode(CullMode::Back);
    }
}
