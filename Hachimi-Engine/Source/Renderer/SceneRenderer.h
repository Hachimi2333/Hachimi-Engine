#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <array>

namespace HachimiEngine
{
    class EnvironmentMap;
    class ShadowMap;

    struct DirectionalLight
    {
        glm::vec3 Direction { -0.5f, -1.0f, -0.3f };
        glm::vec3 Color { 1.0f, 0.98f, 0.95f };
        float Intensity = 1.4f;
        bool CastsShadows = true;
        float ShadowBias = 0.0005f;
    };

    struct PointLight
    {
        glm::vec3 Position { 3.0f, 4.0f, 2.0f };
        glm::vec3 Color { 1.0f, 0.9f, 0.7f };
        float Intensity = 12.0f;
        float Range = 12.0f;
    };

    struct LightingEnvironment
    {
        DirectionalLight Directional;
        std::array<PointLight, 4> PointLights;
        int PointLightCount = 1;
        glm::vec3 AmbientColor { 0.08f, 0.08f, 0.10f };
        float AmbientIntensity = 1.0f;
    };

    // Immediate-mode forward scene renderer used by the editor and runtime scenes.
    class SceneRenderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition);
        static void SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const Ref<Material>& material);
        static void DrawGrid(float size = 20.0f, uint32_t divisions = 20);
        static void DrawSkybox();
        static void EndScene();

        static void BeginDirectionalShadowPass(const glm::mat4& lightViewProjection);
        static void SubmitShadowMesh(const Ref<Mesh>& mesh, const glm::mat4& transform);
        static void EndDirectionalShadowPass();

        static glm::mat4 CalculateDirectionalLightViewProjection(const glm::vec3& cameraPosition);

        static LightingEnvironment& GetLightingEnvironment() { return s_Lighting; }
        static EnvironmentSettings& GetEnvironmentSettings() { return s_Environment; }
        static Ref<Material> GetDefaultMaterial() { return s_DefaultMaterial; }

    private:
        static void UploadLighting(const Ref<Shader>& shader, const glm::vec3& cameraPosition);

    private:
        static Ref<Shader> s_DefaultShader;
        static Ref<Shader> s_GridShader;
        static Ref<Shader> s_DirectionalShadowShader;
        static Ref<Shader> s_SkyboxShader;
        static Ref<Material> s_DefaultMaterial;
        static Ref<Mesh> s_GridMesh;
        static Ref<Mesh> s_SkyboxMesh;
        static Ref<ShadowMap> s_DirectionalShadowMap;
        static Ref<EnvironmentMap> s_EnvironmentMap;
        static LightingEnvironment s_Lighting;
        static EnvironmentSettings s_Environment;
        static glm::mat4 s_ViewProjection;
        static glm::mat4 s_View;
        static glm::mat4 s_Projection;
        static glm::vec3 s_CameraPosition;
        static glm::vec3 s_CameraForward;
        static glm::mat4 s_DirectionalLightViewProjection;
        static bool s_DirectionalShadowEnabled;
    };
}
