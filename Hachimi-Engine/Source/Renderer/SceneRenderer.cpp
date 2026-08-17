#include "Renderer/SceneRenderer.h"

#include "Core/Assert.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShadowMap.h"
#include "Math/Math.h"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <limits>

namespace HachimiEngine
{
    Ref<Shader> SceneRenderer::s_DefaultShader;
    Ref<Shader> SceneRenderer::s_GridShader;
    Ref<Shader> SceneRenderer::s_DirectionalShadowShader;
    Ref<Shader> SceneRenderer::s_SkyboxShader;
    Ref<Material> SceneRenderer::s_DefaultMaterial;
    Ref<Mesh> SceneRenderer::s_GridMesh;
    Ref<Mesh> SceneRenderer::s_SkyboxMesh;
    Ref<ShadowMap> SceneRenderer::s_DirectionalShadowMap;
    Ref<EnvironmentMap> SceneRenderer::s_EnvironmentMap;
    LightingEnvironment SceneRenderer::s_Lighting;
    EnvironmentSettings SceneRenderer::s_Environment;
    Math::Mat4 SceneRenderer::s_ViewProjection { 1.0f };
    Math::Mat4 SceneRenderer::s_View { 1.0f };
    Math::Mat4 SceneRenderer::s_Projection { 1.0f };
    Math::Vec3 SceneRenderer::s_CameraPosition { 0.0f };
    Math::Vec3 SceneRenderer::s_CameraForward { 0.0f, 0.0f, -1.0f };
    Math::Mat4 SceneRenderer::s_DirectionalLightViewProjection { 1.0f };
    bool SceneRenderer::s_DirectionalShadowEnabled = false;
    bool SceneRenderer::s_DirectionalShadowPassActive = false;

    void SceneRenderer::Init()
    {
        s_DefaultShader = Shader::CreateEngineShader("Default.glsl");
        s_GridShader = Shader::CreateEngineShader("Grid.glsl");
        s_DirectionalShadowShader = Shader::CreateEngineShader("DirectionalShadow.glsl");
        s_SkyboxShader = Shader::CreateEngineShader("Skybox.glsl");
        s_DefaultMaterial = Material::Create(s_DefaultShader);
        s_GridMesh = MeshFactory::CreateGrid();
        s_SkyboxMesh = MeshFactory::CreateCube(2.0f);
        s_DirectionalShadowMap = ShadowMap::Create(2048, 2048);
        s_EnvironmentMap = CreateRef<EnvironmentMap>(128);
    }

    void SceneRenderer::Shutdown()
    {
        s_EnvironmentMap.reset();
        s_DirectionalShadowMap.reset();
        s_SkyboxMesh.reset();
        s_GridMesh.reset();
        s_DefaultMaterial.reset();
        s_DefaultShader.reset();
        s_GridShader.reset();
        s_DirectionalShadowShader.reset();
        s_SkyboxShader.reset();
        s_ViewProjection = Math::Mat4(1.0f);
        s_View = Math::Mat4(1.0f);
        s_Projection = Math::Mat4(1.0f);
        s_CameraPosition = Math::Vec3(0.0f);
        s_CameraForward = Math::Vec3(0.0f, 0.0f, -1.0f);
        s_DirectionalLightViewProjection = Math::Mat4(1.0f);
        s_DirectionalShadowEnabled = false;
        s_DirectionalShadowPassActive = false;
    }

    void SceneRenderer::BeginScene(const EditorCamera& camera)
    {
        BeginScene(camera.GetViewMatrix(), camera.GetProjection(), camera.GetPosition());
    }

    void SceneRenderer::BeginScene(const Math::Mat4& view, const Math::Mat4& projection, const Math::Vec3& cameraPosition)
    {
        s_ViewProjection = projection * view;
        s_View = view;
        s_Projection = projection;
        s_CameraPosition = cameraPosition;

        const Math::Mat4 transposedView = Math::Transpose(view);
        s_CameraForward = Math::Normalize(-Math::Vec3(transposedView[2]));

        // No shadow data has been rendered for this scene yet.
        s_DirectionalShadowEnabled = false;

        PostProcessPass::SetExposure(s_Environment.Exposure);
    }

    void SceneRenderer::SubmitMesh(const Ref<Mesh>& mesh, const Math::Mat4& transform, const Ref<Material>& material)
    {
        const Ref<Material>& drawMaterial = material != nullptr ? material : s_DefaultMaterial;
        drawMaterial->Bind();

        const Ref<Shader>& shader = drawMaterial->GetShader();
        HE_CORE_ASSERT(shader != nullptr);

        shader->SetMat4("u_ViewProjection", s_ViewProjection);
        shader->SetMat4("u_Model", transform);
        UploadLighting(shader, s_CameraPosition);

        shader->SetInt("u_DirectionalShadowEnabled", s_DirectionalShadowEnabled ? 1 : 0);
        shader->SetMat4("u_DirectionalLightViewProjection", s_DirectionalLightViewProjection);
        shader->SetFloat("u_DirectionalShadowBias", s_Lighting.Directional.ShadowBias);
        shader->SetInt("u_DirectionalShadowMap", 1);
        if (s_DirectionalShadowMap != nullptr && s_DirectionalShadowMap->GetDepthTextureRendererID() != 0)
        {
            glBindTextureUnit(1, s_DirectionalShadowMap->GetDepthTextureRendererID());
        }

        if (s_EnvironmentMap != nullptr)
        {
            shader->SetInt("u_IrradianceMap", 2);
            shader->SetInt("u_PrefilteredMap", 3);
            shader->SetFloat("u_EnvironmentIntensity", s_Environment.EnvironmentIntensity);
            s_EnvironmentMap->BindIrradiance(2);
            s_EnvironmentMap->BindPrefiltered(3);
        }
        else
        {
            shader->SetFloat("u_EnvironmentIntensity", 0.0f);
        }

        const DrawMode drawMode = mesh->GetDrawMode() == MeshDrawMode::Lines ? DrawMode::Lines : DrawMode::Triangles;
        Renderer::DrawIndexed(mesh->GetVertexArray(), 0, drawMode);
    }

    void SceneRenderer::DrawGrid(float size, uint32_t divisions)
    {
        HE_CORE_ASSERT(s_GridMesh != nullptr);

        if (size != 20.0f || divisions != 20)
        {
            s_GridMesh = MeshFactory::CreateGrid(size, divisions);
        }

        s_GridShader->Bind();
        s_GridShader->SetMat4("u_ViewProjection", s_ViewProjection);
        Renderer::DrawIndexed(s_GridMesh->GetVertexArray(), 0, DrawMode::Lines);
    }

    void SceneRenderer::DrawSkybox()
    {
        if (!s_Environment.ShowSkybox || s_EnvironmentMap == nullptr || s_SkyboxMesh == nullptr)
        {
            return;
        }

        // Draw the sky first without depth testing; later geometry simply overwrites it.
        Renderer::SetDepthTest(false);
        s_SkyboxShader->Bind();

        const Math::Mat4 skyViewProjection = s_Projection * Math::Mat4(Math::Mat3(s_View));
        s_SkyboxShader->SetMat4("u_ViewProjection", skyViewProjection);
        s_SkyboxShader->SetInt("u_SkyboxTexture", 0);
        s_SkyboxShader->SetFloat("u_SkyboxIntensity", s_Environment.EnvironmentIntensity);
        s_EnvironmentMap->BindSkybox(0);

        Renderer::DrawIndexed(s_SkyboxMesh->GetVertexArray(), 0, DrawMode::Triangles);
        Renderer::SetDepthTest(true);
    }

    void SceneRenderer::EndScene()
    {
    }

    void SceneRenderer::BeginDirectionalShadowPass(const Math::Mat4& lightViewProjection)
    {
        HE_CORE_ASSERT(s_DirectionalShadowMap != nullptr);
        HE_CORE_ASSERT(s_DirectionalShadowShader != nullptr);

        s_DirectionalShadowMap->BindForWriting();
        s_DirectionalLightViewProjection = lightViewProjection;
        s_DirectionalShadowEnabled = true;
        s_DirectionalShadowPassActive = true;

        s_DirectionalShadowShader->Bind();
        s_DirectionalShadowShader->SetMat4("u_ViewProjection", lightViewProjection);
        Renderer::SetPolygonOffset(true, 1.0f, 1.0f);
    }

    void SceneRenderer::SubmitShadowMesh(const Ref<Mesh>& mesh, const Math::Mat4& transform)
    {
        HE_CORE_ASSERT(s_DirectionalShadowPassActive);

        if (mesh == nullptr || mesh->GetDrawMode() != MeshDrawMode::Triangles)
        {
            return;
        }

        s_DirectionalShadowShader->SetMat4("u_Model", transform);
        Renderer::DrawIndexed(mesh->GetVertexArray(), 0, DrawMode::Triangles);
    }

    void SceneRenderer::EndDirectionalShadowPass()
    {
        Renderer::SetPolygonOffset(false);
        s_DirectionalShadowPassActive = false;
        s_DirectionalShadowMap->Unbind();

        // Keep s_DirectionalShadowEnabled set so SubmitMesh samples the map
        // rendered above during the current scene pass.
    }

    Math::Mat4 SceneRenderer::CalculateDirectionalLightViewProjection(const Math::Vec3& cameraPosition)
    {
        const Math::Vec3 lightDirection = Math::Normalize(s_Lighting.Directional.Direction);
        constexpr float shadowDistance = 30.0f;

        // Center the shadow volume between the camera and the area it is looking at.
        const Math::Vec3 center = cameraPosition + s_CameraForward * (shadowDistance * 0.5f);
        const Math::Vec3 lightPosition = center - lightDirection * shadowDistance;
        const Math::Vec3 upDirection = std::abs(lightDirection.y) > 0.99f
            ? Math::Vec3(1.0f, 0.0f, 0.0f)
            : Math::Vec3(0.0f, 1.0f, 0.0f);

        const Math::Mat4 lightView = Math::LookAt(lightPosition, center, upDirection);

        std::array<Math::Vec3, 8> corners =
        {
            center + Math::Vec3(-shadowDistance, -shadowDistance, -shadowDistance),
            center + Math::Vec3( shadowDistance, -shadowDistance, -shadowDistance),
            center + Math::Vec3(-shadowDistance,  shadowDistance, -shadowDistance),
            center + Math::Vec3( shadowDistance,  shadowDistance, -shadowDistance),
            center + Math::Vec3(-shadowDistance, -shadowDistance,  shadowDistance),
            center + Math::Vec3( shadowDistance, -shadowDistance,  shadowDistance),
            center + Math::Vec3(-shadowDistance,  shadowDistance,  shadowDistance),
            center + Math::Vec3( shadowDistance,  shadowDistance,  shadowDistance)
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

    void SceneRenderer::UploadLighting(const Ref<Shader>& shader, const Math::Vec3& cameraPosition)
    {
        shader->SetFloat3("u_CameraPosition", cameraPosition);
        shader->SetFloat3("u_AmbientColor", s_Lighting.AmbientColor);
        shader->SetFloat("u_AmbientIntensity", s_Lighting.AmbientIntensity);
        shader->SetFloat3("u_DirectionalLightDirection", s_Lighting.Directional.Direction);
        shader->SetFloat3("u_DirectionalLightColor", s_Lighting.Directional.Color);
        shader->SetFloat("u_DirectionalLightIntensity", s_Lighting.Directional.Intensity);
        shader->SetInt("u_PointLightCount", s_Lighting.PointLightCount);

        for (int i = 0; i < 4; ++i)
        {
            const std::string indexString = std::to_string(i);
            shader->SetFloat3("u_PointLights[" + indexString + "].Position", s_Lighting.PointLights[static_cast<size_t>(i)].Position);
            shader->SetFloat3("u_PointLights[" + indexString + "].Color", s_Lighting.PointLights[static_cast<size_t>(i)].Color);
            shader->SetFloat("u_PointLights[" + indexString + "].Intensity", s_Lighting.PointLights[static_cast<size_t>(i)].Intensity);
            shader->SetFloat("u_PointLights[" + indexString + "].Range", s_Lighting.PointLights[static_cast<size_t>(i)].Range);
        }
    }
}
