#include "Renderer/Passes/OpaquePass.h"

#include "Core/Assert.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/FrameUniforms.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialResolver.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"
#include "Renderer/ShadowMap.h"
#include "Renderer/UniformBuffer.h"

namespace HachimiEngine
{
    namespace
    {
        // Texture units the scene shader declares, in the order Material binds its own.
        constexpr int ShadowMapTextureUnit = 1;
        constexpr int IrradianceTextureUnit = 2;
        constexpr int PrefilteredTextureUnit = 3;
    }

    void OpaquePass::Execute(RenderPassContext& context)
    {
        const RenderView& view = context.View;
        if (view.Items.empty())
        {
            return;
        }

        // Per-view constants go over the wire once, not once per mesh: the view-projection,
        // camera, light rig and environment used to cost around 26 uniform uploads per draw.
        UniformBuffer& frameUniforms = context.Renderers.GetFrameUniforms();
        const FrameUniforms frame = FrameUniforms::FromView(
            view,
            context.DirectionalLightViewProjection,
            context.DirectionalShadowEnabled);
        frameUniforms.SetData(&frame, static_cast<uint32_t>(sizeof(FrameUniforms)));
        frameUniforms.Bind();

        MeshLibrary& meshes = context.Renderers.GetMeshes();
        MaterialResolver& materials = context.Renderers.GetMaterials();
        EnvironmentMap& environmentMap = context.Renderers.GetEnvironmentMap();

        if (context.DirectionalShadowEnabled)
        {
            const uint32_t shadowTexture = context.Renderers.GetShadowMap().GetDepthTextureRendererID();
            if (shadowTexture != 0)
            {
                Renderer::BindTextureUnit(ShadowMapTextureUnit, shadowTexture);
            }
        }

        if (environmentMap.HasContent())
        {
            environmentMap.BindIrradiance(IrradianceTextureUnit);
            environmentMap.BindPrefiltered(PrefilteredTextureUnit);
        }

        const Ref<Shader>& defaultShader = context.Renderers.GetDefaultShader();
        HE_CORE_ASSERT(defaultShader != nullptr);

        // Sampler units are view-constant, so they are set once per program rather than per
        // draw. Tracked here so a material with its own shader still gets them.
        Ref<Shader> activatedShader;
        const auto activateShader = [&activatedShader](const Ref<Shader>& shader)
        {
            if (shader == activatedShader)
            {
                return;
            }

            shader->Bind();
            shader->SetInt("u_DirectionalShadowMap", ShadowMapTextureUnit);
            shader->SetInt("u_IrradianceMap", IrradianceTextureUnit);
            shader->SetInt("u_PrefilteredMap", PrefilteredTextureUnit);
            activatedShader = shader;
        };

        for (const RenderItem& item : view.Items)
        {
            const Ref<Mesh> gpuMesh = meshes.GetOrCreate(item.Mesh);
            if (gpuMesh == nullptr)
            {
                continue;
            }

            // The item carries a material reference, not a material: resolving it here is what
            // keeps the scene extraction free of GPU work.
            const Ref<Material> material = materials.Resolve(item.Material);
            const Ref<Shader>& shader = material != nullptr ? material->GetShader() : defaultShader;
            HE_CORE_ASSERT(shader != nullptr);

            activateShader(shader);

            if (material != nullptr)
            {
                // Re-applies the same program and sets the albedo texture state.
                material->Bind();
            }
            else
            {
                shader->SetInt("u_HasAlbedoTexture", 0);
            }

            shader->SetMat4("u_Model", item.Transform);
            shader->SetFloat4("u_AlbedoColor", item.AlbedoColor);
            shader->SetFloat("u_Roughness", item.Roughness);
            shader->SetFloat("u_Metallic", item.Metallic);

            const DrawMode drawMode = gpuMesh->GetDrawMode() == MeshDrawMode::Lines ? DrawMode::Lines : DrawMode::Triangles;
            Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, drawMode);
        }
    }
}
