#include "Renderer/SceneRenderer.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/ShadowMap.h"
#include "Math/Math.h"

#include <array>
#include <limits>

namespace HachimiEngine
{
    namespace
    {
        // Span of the shadow volume around the camera, in world units.
        constexpr float ShadowDistance = 30.0f;

        // Point light 0 is bound to a fixed texture unit, and each additional light costs
        // a group of uniforms, so the shader declares exactly LightingEnvironment::MaxPointLights.
        constexpr int ShadowMapTextureUnit = 1;
        constexpr int IrradianceTextureUnit = 2;
        constexpr int PrefilteredTextureUnit = 3;
    }

    SceneRenderer::SceneRenderer(RendererContext& context)
        : m_Context(context)
    {
    }

    void SceneRenderer::Render(const RenderView& view)
    {
        HE_CORE_ASSERT(m_Context.IsInitialized());

        FrameState frame;
        frame.View = view.View;
        frame.Projection = view.Projection;
        frame.ViewProjection = view.ViewProjection;
        frame.CameraPosition = view.CameraPosition;
        frame.CameraForward = view.CameraForward;

        const DirectionalLight& directional = view.Lighting.Directional;
        const bool castsDirectionalShadows = directional.CastsShadows
            && directional.Intensity > 0.0f
            && Math::Length(directional.Direction) > 0.001f;

        if (castsDirectionalShadows)
        {
            frame.DirectionalLightViewProjection = CalculateDirectionalLightViewProjection(frame, directional);
            frame.DirectionalShadowEnabled = true;
            DrawDirectionalShadowPass(view, frame);
        }

        DrawSkybox(view, frame);

        if (view.DrawGrid)
        {
            DrawGrid(frame);
        }

        DrawItems(view, frame);
    }

    void SceneRenderer::DrawDirectionalShadowPass(const RenderView& view, const FrameState& frame)
    {
        ShadowMap& shadowMap = m_Context.GetShadowMap();
        const Ref<Shader>& shader = m_Context.GetDirectionalShadowShader();
        HE_CORE_ASSERT(shader != nullptr);

        shadowMap.BindForWriting();

        shader->Bind();
        shader->SetMat4("u_ViewProjection", frame.DirectionalLightViewProjection);
        Renderer::SetPolygonOffset(true, 1.0f, 1.0f);

        for (const RenderItem& item : view.Items)
        {
            if (item.Mesh == nullptr || item.Mesh->GetDrawMode() != MeshDrawMode::Triangles)
            {
                continue;
            }

            const Ref<Mesh> gpuMesh = m_Context.GetMeshes().GetOrCreate(item.Mesh);
            if (gpuMesh == nullptr)
            {
                continue;
            }

            shader->SetMat4("u_Model", item.Transform);
            Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Triangles);
        }

        Renderer::SetPolygonOffset(false);
        shadowMap.Unbind();

        // frame.DirectionalShadowEnabled stays set so the scene pass samples the map
        // rendered above.
    }

    void SceneRenderer::DrawSkybox(const RenderView& view, const FrameState& frame)
    {
        if (!view.Environment.ShowSkybox || !m_Context.GetEnvironmentMap().HasContent())
        {
            return;
        }

        const Ref<Mesh> gpuMesh = m_Context.GetMeshes().GetOrCreate(m_Context.GetSkyboxMesh());
        if (gpuMesh == nullptr)
        {
            return;
        }

        // Draw the sky first without depth testing; later geometry simply overwrites it.
        Renderer::SetDepthTest(false);
        const Ref<Shader>& shader = m_Context.GetSkyboxShader();
        shader->Bind();

        const Math::Mat4 skyViewProjection = frame.Projection * Math::Mat4(Math::Mat3(frame.View));
        shader->SetMat4("u_ViewProjection", skyViewProjection);
        shader->SetInt("u_SkyboxTexture", 0);
        shader->SetFloat("u_SkyboxIntensity", view.Environment.EnvironmentIntensity);
        m_Context.GetEnvironmentMap().BindSkybox(0);

        Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Triangles);
        Renderer::SetDepthTest(true);
    }

    void SceneRenderer::DrawGrid(const FrameState& frame)
    {
        const Ref<Mesh> gpuMesh = m_Context.GetMeshes().GetOrCreate(m_Context.GetGridMesh());
        if (gpuMesh == nullptr)
        {
            return;
        }

        const Ref<Shader>& shader = m_Context.GetGridShader();
        shader->Bind();
        shader->SetMat4("u_ViewProjection", frame.ViewProjection);
        Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Lines);
    }

    void SceneRenderer::DrawItems(const RenderView& view, const FrameState& frame)
    {
        for (const RenderItem& item : view.Items)
        {
            SubmitMesh(view, frame, item);
        }
    }

    void SceneRenderer::SubmitMesh(const RenderView& view, const FrameState& frame, const RenderItem& item)
    {
        const Ref<Mesh> gpuMesh = m_Context.GetMeshes().GetOrCreate(item.Mesh);
        if (gpuMesh == nullptr)
        {
            return;
        }

        // An explicit material supplies the shader and the albedo texture; the surface
        // values stay with the entity, so no per-entity Material instance is needed.
        const Ref<Shader> shader = item.Material != nullptr
            ? item.Material->GetShader()
            : m_Context.GetDefaultShader();
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

        shader->SetMat4("u_ViewProjection", frame.ViewProjection);
        shader->SetMat4("u_Model", item.Transform);
        shader->SetFloat4("u_AlbedoColor", item.AlbedoColor);
        shader->SetFloat("u_Roughness", item.Roughness);
        shader->SetFloat("u_Metallic", item.Metallic);

        UploadLighting(shader, frame, view.Lighting);

        shader->SetInt("u_DirectionalShadowEnabled", frame.DirectionalShadowEnabled ? 1 : 0);
        shader->SetMat4("u_DirectionalLightViewProjection", frame.DirectionalLightViewProjection);
        shader->SetFloat("u_DirectionalShadowBias", view.Lighting.Directional.ShadowBias);

        if (frame.DirectionalShadowEnabled)
        {
            const uint32_t shadowTexture = m_Context.GetShadowMap().GetDepthTextureRendererID();
            if (shadowTexture != 0)
            {
                shader->SetInt("u_DirectionalShadowMap", ShadowMapTextureUnit);
                Renderer::BindTextureUnit(ShadowMapTextureUnit, shadowTexture);
            }
        }

        if (m_Context.GetEnvironmentMap().HasContent())
        {
            shader->SetInt("u_IrradianceMap", IrradianceTextureUnit);
            shader->SetInt("u_PrefilteredMap", PrefilteredTextureUnit);
            shader->SetFloat("u_EnvironmentIntensity", view.Environment.EnvironmentIntensity);
            m_Context.GetEnvironmentMap().BindIrradiance(IrradianceTextureUnit);
            m_Context.GetEnvironmentMap().BindPrefiltered(PrefilteredTextureUnit);
        }
        else
        {
            shader->SetFloat("u_EnvironmentIntensity", 0.0f);
        }

        const DrawMode drawMode = gpuMesh->GetDrawMode() == MeshDrawMode::Lines ? DrawMode::Lines : DrawMode::Triangles;
        Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, drawMode);
    }

    Math::Mat4 SceneRenderer::CalculateDirectionalLightViewProjection(const FrameState& frame, const DirectionalLight& light) const
    {
        const Math::Vec3 lightDirection = Math::Normalize(light.Direction);

        // Center the shadow volume between the camera and the area it is looking at.
        const Math::Vec3 center = frame.CameraPosition + frame.CameraForward * (ShadowDistance * 0.5f);
        const Math::Vec3 lightPosition = center - lightDirection * ShadowDistance;
        const Math::Vec3 upDirection = std::abs(lightDirection.y) > 0.99f
            ? Math::Vec3(1.0f, 0.0f, 0.0f)
            : Math::Vec3(0.0f, 1.0f, 0.0f);

        const Math::Mat4 lightView = Math::LookAt(lightPosition, center, upDirection);

        std::array<Math::Vec3, 8> corners =
        {
            center + Math::Vec3(-ShadowDistance, -ShadowDistance, -ShadowDistance),
            center + Math::Vec3( ShadowDistance, -ShadowDistance, -ShadowDistance),
            center + Math::Vec3(-ShadowDistance,  ShadowDistance, -ShadowDistance),
            center + Math::Vec3( ShadowDistance,  ShadowDistance, -ShadowDistance),
            center + Math::Vec3(-ShadowDistance, -ShadowDistance,  ShadowDistance),
            center + Math::Vec3( ShadowDistance, -ShadowDistance,  ShadowDistance),
            center + Math::Vec3(-ShadowDistance,  ShadowDistance,  ShadowDistance),
            center + Math::Vec3( ShadowDistance,  ShadowDistance,  ShadowDistance)
        };

        Math::Vec3 minimum(std::numeric_limits<float>::max());
        Math::Vec3 maximum(std::numeric_limits<float>::lowest());
        for (const Math::Vec3& corner : corners)
        {
            const Math::Vec3 lightSpaceCorner = lightView * Math::Vec4(corner, 1.0f);
            minimum = Math::Min(minimum, lightSpaceCorner);
            maximum = Math::Max(maximum, lightSpaceCorner);
        }

        // View-space z is negative in front of the light camera; flip the range for Math::Ortho.
        const Math::Mat4 lightProjection = Math::Ortho(
            minimum.x,
            maximum.x,
            minimum.y,
            maximum.y,
            -maximum.z,
            -minimum.z);

        return lightProjection * lightView;
    }

    void SceneRenderer::UploadLighting(const Ref<Shader>& shader, const FrameState& frame, const LightingEnvironment& lighting)
    {
        shader->SetFloat3("u_CameraPosition", frame.CameraPosition);
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
