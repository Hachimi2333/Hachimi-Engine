#include "Renderer/SceneRenderer.h"

#include "Core/Assert.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/type_ptr.hpp>

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
    v_Normal = mat3(u_Model) * a_Normal;
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
uniform vec3 u_DirectionalLightDirection;
uniform vec3 u_DirectionalLightColor;
uniform float u_DirectionalLightIntensity;

struct PointLight
{
    vec3 Position;
    vec3 Color;
    float Intensity;
};
uniform PointLight u_PointLights[4];
uniform int u_PointLightCount;

vec3 CalculateLight(vec3 normal, vec3 viewDirection, vec3 lightDirection, vec3 lightColor, vec3 albedo)
{
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float shininess = (1.0 - u_Roughness) * 64.0 + 1.0;
    float specular = pow(max(dot(normal, halfDirection), 0.0), shininess);
    return lightColor * (albedo * diffuse + vec3(specular));
}

void main()
{
    vec4 sampledAlbedo = u_HasAlbedoTexture == 1 ? texture(u_AlbedoTexture, v_TexCoord) : vec4(1.0);
    vec3 albedo = sampledAlbedo.rgb * u_AlbedoColor.rgb * v_Color.rgb;
    vec3 normal = normalize(v_Normal);
    vec3 viewDirection = normalize(u_CameraPosition - v_WorldPosition);

    vec3 lighting = albedo * 0.08;

    vec3 directionalDirection = normalize(-u_DirectionalLightDirection);
    lighting += CalculateLight(normal, viewDirection, directionalDirection, u_DirectionalLightColor, albedo) * u_DirectionalLightIntensity;

    for (int i = 0; i < u_PointLightCount && i < 4; ++i)
    {
        vec3 offset = u_PointLights[i].Position - v_WorldPosition;
        float distanceSquared = max(dot(offset, offset), 0.01);
        float attenuation = u_PointLights[i].Intensity / distanceSquared;
        lighting += CalculateLight(normal, viewDirection, normalize(offset), u_PointLights[i].Color, albedo) * attenuation;
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
    }

    Ref<Shader> SceneRenderer::s_DefaultShader;
    Ref<Shader> SceneRenderer::s_GridShader;
    Ref<Material> SceneRenderer::s_DefaultMaterial;
    Ref<Mesh> SceneRenderer::s_GridMesh;
    LightingEnvironment SceneRenderer::s_Lighting;
    glm::mat4 SceneRenderer::s_ViewProjection { 1.0f };
    glm::vec3 SceneRenderer::s_CameraPosition { 0.0f };

    void SceneRenderer::Init()
    {
        s_DefaultShader = Shader::Create("SceneRenderer", DefaultVertexShader, DefaultFragmentShader);
        s_GridShader = Shader::Create("Grid", GridVertexShader, GridFragmentShader);
        s_DefaultMaterial = Material::Create(s_DefaultShader);
        s_GridMesh = MeshFactory::CreateGrid();
    }

    void SceneRenderer::Shutdown()
    {
        s_GridMesh.reset();
        s_DefaultMaterial.reset();
        s_DefaultShader.reset();
        s_GridShader.reset();
        s_ViewProjection = glm::mat4(1.0f);
        s_CameraPosition = glm::vec3(0.0f);
    }

    void SceneRenderer::BeginScene(const EditorCamera& camera)
    {
        BeginScene(camera.GetViewMatrix(), camera.GetProjection(), camera.GetPosition());
    }

    void SceneRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition)
    {
        s_ViewProjection = projection * view;
        s_CameraPosition = cameraPosition;
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

    void SceneRenderer::EndScene()
    {
    }

    void SceneRenderer::UploadLighting(const Ref<Shader>& shader, const glm::vec3& cameraPosition)
    {
        shader->SetFloat3("u_CameraPosition", cameraPosition);
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
        }
    }
}
