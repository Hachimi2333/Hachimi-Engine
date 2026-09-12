#include "Renderer/Passes/GridPass.h"

#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"

namespace HachimiEngine
{
    void GridPass::Execute(RenderPassContext& context)
    {
        if (!context.View.DrawGrid)
        {
            return;
        }

        const Ref<Mesh> gpuMesh = context.Renderers.GetMeshes().GetOrCreate(context.Renderers.GetGridMesh());
        if (gpuMesh == nullptr)
        {
            return;
        }

        const Ref<Shader> shader = context.Renderers.GetGridShader();
        shader->Bind();
        shader->SetMat4("u_ViewProjection", context.View.ViewProjection);

        Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Lines);
    }
}
