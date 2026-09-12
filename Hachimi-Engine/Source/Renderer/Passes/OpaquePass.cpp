#include "Renderer/Passes/OpaquePass.h"

#include "Core/Assert.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"
#include "Renderer/ShadowMap.h"

#include <string>

namespace HachimiEngine
{
    namespace
    {
        // Texture units the scene shader declares, in the order Material binds its own.
        constexpr int ShadowMapTextureUnit = 1;
        constexpr int IrradianceTextureUnit = 2;
        constexpr int PrefilteredTextureUnit = 3;

        void UploadLighting(const Ref<Shader>& shader, const RenderView& view)
        {
            const LightingEnvironment& lighting = view.Lighting;

            shader->SetFloat3("u_CameraPosition", view.CameraPosition);
            shader->SetFloat3("u_AmbientColor", lighting.AmbientColor);
            shader->SetFloat("u_AmbientIntensity", lighting.AmbientIntensity);
            shader->SetFloat3("u_DirectionalLightDirection", lighting.Directional.Direction);
            shader->SetFloat3("u_DirectionalLightColor", lighting.Directional.Color);
            shader->SetFloat("u_DirectionalLightIntensity", lighting.Directional.Intensity);
            shader->SetInt("u_PointLightCount", lighting.PointLightCount);

            for (size_t index = 0; index < lighting.PointLights.size(); ++index)
            {
                const PointLight& pointLight = lighting.PointLights[index];
                const std::string indexString = std::to_string(index);
                shader->SetFloat3("u_PointLights[" + indexString + "].Position", pointLight.Position);
                shader->SetFloat3("u_PointLights[" + indexString + "].Color", pointLight.Color);
                shader->SetFloat("u_PointLights[" + indexString + "].Intensity", pointLight.Intensity);
                shader->SetFloat("u_PointLights[" + indexString + "].Range", pointLight.Range);
            }
        }
    }

    void OpaquePass::Execute(RenderPassContext& context)
    {
        const RenderView& view = context.View;
        if (view.Items.empty())
        {
            return;
        }

        MeshLibrary& meshes = context.Renderers.GetMeshes();
        EnvironmentMap& environmentMap = context.Renderers.GetEnvironmentMap();

        for (const RenderItem& item : view.Items)
        {
            const Ref<Mesh> gpuMesh = meshes.GetOrCreate(item.Mesh);
            if (gpuMesh == nullptr)
            {
                continue;
            }

            // An explicit material supplies the shader and the albedo texture; the surface
            // values stay with the entity, so no per-entity Material instance is needed.
            const Ref<Shader> shader = item.Material != nullptr
                ? item.Material->GetShader()
                : context.Renderers.GetDefaultShader();
            HE_CORE_ASSERT(shader != nullptr);

            if (item.Material != nullptr)
            {
                item.Material->Bind();
            }
            else
            {
                shader->Bind();
                shader->SetInt("u_HasAlbedoTexture", 0);
            }

            shader->SetMat4("u_ViewProjection", view.ViewProjection);
            shader->SetMat4("u_Model", item.Transform);
            shader->SetFloat4("u_AlbedoColor", item.AlbedoColor);
            shader->SetFloat("u_Roughness", item.Roughness);
            shader->SetFloat("u_Metallic", item.Metallic);

            UploadLighting(shader, view);

            shader->SetInt("u_DirectionalShadowEnabled", context.DirectionalShadowEnabled ? 1 : 0);
            shader->SetFloat("u_DirectionalShadowBias", view.Lighting.Directional.ShadowBias);

            if (context.DirectionalShadowEnabled)
            {
                const uint32_t shadowTexture = context.Renderers.GetShadowMap().GetDepthTextureRendererID();
                if (shadowTexture != 0)
                {
                    shader->SetMat4("u_DirectionalLightViewProjection", context.DirectionalLightViewProjection);
                    shader->SetInt("u_DirectionalShadowMap", ShadowMapTextureUnit);
                    Renderer::BindTextureUnit(ShadowMapTextureUnit, shadowTexture);
                }
            }

            if (environmentMap.HasContent())
            {
                shader->SetInt("u_IrradianceMap", IrradianceTextureUnit);
                shader->SetInt("u_PrefilteredMap", PrefilteredTextureUnit);
                shader->SetFloat("u_EnvironmentIntensity", view.Environment.EnvironmentIntensity);
                environmentMap.BindIrradiance(IrradianceTextureUnit);
                environmentMap.BindPrefiltered(PrefilteredTextureUnit);
            }
            else
            {
                shader->SetFloat("u_EnvironmentIntensity", 0.0f);
            }

            const DrawMode drawMode = gpuMesh->GetDrawMode() == MeshDrawMode::Lines ? DrawMode::Lines : DrawMode::Triangles;
            Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, drawMode);
        }
    }
}
