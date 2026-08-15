#include "Renderer/SceneRenderer.h"

#include "Core/Assert.h"
#include "Renderer/EnvironmentMap.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShadowMap.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* DefaultVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_WorldPosition;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);
    v_WorldPosition = worldPosition.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    v_Color = a_Color;
    gl_Position = u_ViewProjection * worldPosition;
}
)";

        constexpr const char* DefaultFragmentShader = R"(
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_AlbedoTexture;
uniform int u_HasAlbedoTexture;

uniform vec4 u_AlbedoColor;
uniform float u_Roughness;
uniform float u_Metallic;

uniform vec3 u_CameraPosition;
uniform vec3 u_AmbientColor;
uniform float u_AmbientIntensity;

uniform vec3 u_DirectionalLightDirection;
uniform vec3 u_DirectionalLightColor;
uniform float u_DirectionalLightIntensity;

uniform sampler2D u_DirectionalShadowMap;
uniform mat4 u_DirectionalLightViewProjection;
uniform int u_DirectionalShadowEnabled;
uniform float u_DirectionalShadowBias;

uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;
uniform float u_EnvironmentIntensity;

struct PointLight
{
    vec3 Position;
    vec3 Color;
    float Intensity;
    float Range;
};
uniform PointLight u_PointLights[4];
uniform int u_PointLightCount;

const float PI = 3.14159265359;

float DistributionGGX(vec3 normal, vec3 halfDirection, float roughness)
{
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float normalDotHalf = max(dot(normal, halfDirection), 0.0);
    float denominator = normalDotHalf * normalDotHalf * (roughness4 - 1.0) + 1.0;
    return roughness4 / (PI * denominator * denominator);
}

float GeometrySchlickGGX(float normalDotDirection, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return normalDotDirection / (normalDotDirection * (1.0 - k) + k);
}

float GeometrySmith(float normalDotView, float normalDotLight, float roughness)
{
    return GeometrySchlickGGX(normalDotView, roughness) * GeometrySchlickGGX(normalDotLight, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculateLight(vec3 normal, vec3 viewDirection, vec3 lightDirection, vec3 radiance, vec3 albedo, float roughness, float metallic)
{
    float normalDotLight = max(dot(normal, lightDirection), 0.0);
    if (normalDotLight <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float normalDotView = max(dot(normal, viewDirection), 0.0);
    float normalDotHalf = max(dot(normal, halfDirection), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float distribution = DistributionGGX(normal, halfDirection, roughness);
    float geometry = GeometrySmith(normalDotView, normalDotLight, roughness);
    vec3 fresnel = FresnelSchlick(max(dot(halfDirection, viewDirection), 0.0), F0);

    vec3 specular = distribution * geometry * fresnel / max(4.0 * normalDotView * normalDotLight, 0.001);
    vec3 diffuseFactor = (vec3(1.0) - fresnel) * (1.0 - metallic);
    return (diffuseFactor * albedo / PI + specular) * radiance * normalDotLight;
}

float CalculateDirectionalShadow(vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
    vec4 lightSpacePosition = u_DirectionalLightViewProjection * vec4(worldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;

    if (projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0)
    {
        return 1.0;
    }

    float currentDepth = projected.z;
    float slopeScale = clamp(1.0 - max(dot(normal, lightDirection), 0.0), 0.0, 1.0);
    float bias = u_DirectionalShadowBias + slopeScale * 0.0015;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_DirectionalShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 sampleCoord = projected.xy + vec2(x, y) * texelSize;
            float shadowDepth = texture(u_DirectionalShadowMap, sampleCoord).r;
            shadow += currentDepth - bias > shadowDepth ? 1.0 : 0.0;
        }
    }

    return 1.0 - shadow / 9.0;
}

void main()
{
    vec4 sampledAlbedo = u_HasAlbedoTexture == 1 ? texture(u_AlbedoTexture, v_TexCoord) : vec4(1.0);
    vec3 albedo = sampledAlbedo.rgb * u_AlbedoColor.rgb * v_Color.rgb;
    vec3 normal = normalize(v_Normal);
    vec3 viewDirection = normalize(u_CameraPosition - v_WorldPosition);

    float roughness = clamp(u_Roughness, 0.04, 1.0);
    float metallic = clamp(u_Metallic, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 lighting;
    if (u_EnvironmentIntensity > 0.0)
    {
        vec3 irradiance = texture(u_IrradianceMap, normal).rgb;
        vec3 diffuseIBL = irradiance * albedo * (1.0 - metallic);

        vec3 reflection = reflect(-viewDirection, normal);
        vec3 prefilteredColor = textureLod(u_PrefilteredMap, reflection, roughness * 3.0).rgb;
        vec3 fresnel = FresnelSchlick(max(dot(normal, viewDirection), 0.0), F0);
        vec3 specularIBL = prefilteredColor * fresnel;

        lighting = (diffuseIBL + specularIBL) * u_EnvironmentIntensity;
    }
    else
    {
        lighting = albedo * u_AmbientColor * u_AmbientIntensity;
    }

    vec3 directionalDirection = normalize(-u_DirectionalLightDirection);
    vec3 directionalRadiance = u_DirectionalLightColor * u_DirectionalLightIntensity;

    float shadowFactor = 1.0;
    if (u_DirectionalShadowEnabled == 1)
    {
        shadowFactor = CalculateDirectionalShadow(v_WorldPosition, normal, directionalDirection);
    }

    lighting += CalculateLight(normal, viewDirection, directionalDirection, directionalRadiance, albedo, roughness, metallic) * shadowFactor;

    for (int i = 0; i < u_PointLightCount && i < 4; ++i)
    {
        vec3 offset = u_PointLights[i].Position - v_WorldPosition;
        float distance = length(offset);
        float range = max(u_PointLights[i].Range, 0.01);
        float distanceSquared = max(distance * distance, 0.01);

        // Smooth range window removes the hard cutoff while keeping lighting local.
        float rangeWindow = pow(clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0), 2.0);
        float attenuation = u_PointLights[i].Intensity * rangeWindow / distanceSquared;

        vec3 pointRadiance = u_PointLights[i].Color * attenuation;
        lighting += CalculateLight(normal, viewDirection, normalize(offset), pointRadiance, albedo, roughness, metallic);
    }

    o_Color = vec4(lighting, sampledAlbedo.a * u_AlbedoColor.a * v_Color.a);
}
)";

        constexpr const char* GridVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

        constexpr const char* GridFragmentShader = R"(
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec4 v_Color;

void main()
{
    o_Color = v_Color;
}
)";

        constexpr const char* DirectionalShadowVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main()
{
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

        constexpr const char* DirectionalShadowFragmentShader = R"(
#version 460 core

void main()
{
}
)";

        constexpr const char* SkyboxVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;

out vec3 v_Direction;

void main()
{
    v_Direction = a_Position;
    vec4 clipPosition = u_ViewProjection * vec4(a_Position, 1.0);
    gl_Position = clipPosition.xyww;
}
)";

        constexpr const char* SkyboxFragmentShader = R"(
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec3 v_Direction;

uniform samplerCube u_SkyboxTexture;
uniform float u_SkyboxIntensity;

void main()
{
    vec3 color = texture(u_SkyboxTexture, normalize(v_Direction)).rgb * u_SkyboxIntensity;
    o_Color = vec4(color, 1.0);
}
)";
    }

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
    glm::mat4 SceneRenderer::s_ViewProjection { 1.0f };
    glm::mat4 SceneRenderer::s_View { 1.0f };
    glm::mat4 SceneRenderer::s_Projection { 1.0f };
    glm::vec3 SceneRenderer::s_CameraPosition { 0.0f };
    glm::vec3 SceneRenderer::s_CameraForward { 0.0f, 0.0f, -1.0f };
    glm::mat4 SceneRenderer::s_DirectionalLightViewProjection { 1.0f };
    bool SceneRenderer::s_DirectionalShadowEnabled = false;

    void SceneRenderer::Init()
    {
        s_DefaultShader = Shader::Create("SceneRenderer", DefaultVertexShader, DefaultFragmentShader);
        s_GridShader = Shader::Create("Grid", GridVertexShader, GridFragmentShader);
        s_DirectionalShadowShader = Shader::Create("DirectionalShadow", DirectionalShadowVertexShader, DirectionalShadowFragmentShader);
        s_SkyboxShader = Shader::Create("Skybox", SkyboxVertexShader, SkyboxFragmentShader);
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
        s_ViewProjection = glm::mat4(1.0f);
        s_View = glm::mat4(1.0f);
        s_Projection = glm::mat4(1.0f);
        s_CameraPosition = glm::vec3(0.0f);
        s_CameraForward = glm::vec3(0.0f, 0.0f, -1.0f);
        s_DirectionalLightViewProjection = glm::mat4(1.0f);
        s_DirectionalShadowEnabled = false;
    }

    void SceneRenderer::BeginScene(const EditorCamera& camera)
    {
        BeginScene(camera.GetViewMatrix(), camera.GetProjection(), camera.GetPosition());
    }

    void SceneRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition)
    {
        s_ViewProjection = projection * view;
        s_View = view;
        s_Projection = projection;
        s_CameraPosition = cameraPosition;

        const glm::mat4 transposedView = glm::transpose(view);
        s_CameraForward = glm::normalize(-glm::vec3(transposedView[2]));

        PostProcessPass::SetExposure(s_Environment.Exposure);
    }

    void SceneRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const Ref<Material>& material)
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

        const glm::mat4 skyViewProjection = s_Projection * glm::mat4(glm::mat3(s_View));
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

    void SceneRenderer::BeginDirectionalShadowPass(const glm::mat4& lightViewProjection)
    {
        HE_CORE_ASSERT(s_DirectionalShadowMap != nullptr);
        HE_CORE_ASSERT(s_DirectionalShadowShader != nullptr);

        s_DirectionalShadowMap->BindForWriting();
        s_DirectionalLightViewProjection = lightViewProjection;
        s_DirectionalShadowEnabled = true;

        s_DirectionalShadowShader->Bind();
        s_DirectionalShadowShader->SetMat4("u_ViewProjection", lightViewProjection);
        Renderer::SetPolygonOffset(true, 1.0f, 1.0f);
    }

    void SceneRenderer::SubmitShadowMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        HE_CORE_ASSERT(s_DirectionalShadowEnabled);

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
        s_DirectionalShadowEnabled = false;
        s_DirectionalShadowMap->Unbind();
    }

    glm::mat4 SceneRenderer::CalculateDirectionalLightViewProjection(const glm::vec3& cameraPosition)
    {
        const glm::vec3 lightDirection = glm::normalize(s_Lighting.Directional.Direction);
        constexpr float shadowDistance = 30.0f;

        // Center the shadow volume between the camera and the area it is looking at.
        const glm::vec3 center = cameraPosition + s_CameraForward * (shadowDistance * 0.5f);
        const glm::vec3 lightPosition = center - lightDirection * shadowDistance;
        const glm::vec3 upDirection = std::abs(lightDirection.y) > 0.99f
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        const glm::mat4 lightView = glm::lookAt(lightPosition, center, upDirection);

        std::array<glm::vec3, 8> corners =
        {
            center + glm::vec3(-shadowDistance, -shadowDistance, -shadowDistance),
            center + glm::vec3( shadowDistance, -shadowDistance, -shadowDistance),
            center + glm::vec3(-shadowDistance,  shadowDistance, -shadowDistance),
            center + glm::vec3( shadowDistance,  shadowDistance, -shadowDistance),
            center + glm::vec3(-shadowDistance, -shadowDistance,  shadowDistance),
            center + glm::vec3( shadowDistance, -shadowDistance,  shadowDistance),
            center + glm::vec3(-shadowDistance,  shadowDistance,  shadowDistance),
            center + glm::vec3( shadowDistance,  shadowDistance,  shadowDistance)
        };

        glm::vec3 minimum(std::numeric_limits<float>::max());
        glm::vec3 maximum(std::numeric_limits<float>::lowest());
        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 lightSpaceCorner = lightView * glm::vec4(corner, 1.0f);
            minimum = glm::min(minimum, lightSpaceCorner);
            maximum = glm::max(maximum, lightSpaceCorner);
        }

        // View-space z is negative in front of the light camera; flip the range for glm::ortho.
        const glm::mat4 lightProjection = glm::ortho(
            minimum.x,
            maximum.x,
            minimum.y,
            maximum.y,
            -maximum.z,
            -minimum.z);

        return lightProjection * lightView;
    }

    void SceneRenderer::UploadLighting(const Ref<Shader>& shader, const glm::vec3& cameraPosition)
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
