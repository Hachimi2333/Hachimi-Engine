#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Math/Math.h"

#include <array>

namespace HachimiEngine
{
    class EnvironmentMap;
    class ShadowMap;

    struct DirectionalLight
    {
        Math::Vec3 Direction { -0.5f, -1.0f, -0.3f };
        Math::Vec3 Color { 1.0f, 0.98f, 0.95f };
        float Intensity = 1.4f;
        bool CastsShadows = true;
        float ShadowBias = 0.0005f;
    };

    struct PointLight
    {
        Math::Vec3 Position { 3.0f, 4.0f, 2.0f };
        Math::Vec3 Color { 1.0f, 0.9f, 0.7f };
        float Intensity = 12.0f;
        float Range = 12.0f;
    };

    struct LightingEnvironment
    {
        DirectionalLight Directional;
        std::array<PointLight, 4> PointLights;
        int PointLightCount = 1;
        Math::Vec3 AmbientColor { 0.08f, 0.08f, 0.10f };
        float AmbientIntensity = 1.0f;
    };

    // Immediate-mode forward scene renderer used by the editor and runtime scenes.
    class SceneRenderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Math::Mat4& view, const Math::Mat4& projection, const Math::Vec3& cameraPosition);
        static void SubmitMesh(const Ref<Mesh>& mesh, const Math::Mat4& transform, const Ref<Material>& material);
        static void DrawGrid(float size = 20.0f, uint32_t divisions = 20);
        static void DrawSkybox();
        static void EndScene();

        static void BeginDirectionalShadowPass(const Math::Mat4& lightViewProjection);
        static void SubmitShadowMesh(const Ref<Mesh>& mesh, const Math::Mat4& transform);
        static void EndDirectionalShadowPass();

        static Math::Mat4 CalculateDirectionalLightViewProjection(const Math::Vec3& cameraPosition);

        static LightingEnvironment& GetLightingEnvironment() { return s_Lighting; }
        static EnvironmentSettings& GetEnvironmentSettings() { return s_Environment; }
        static Ref<Material> GetDefaultMaterial() { return s_DefaultMaterial; }

    private:
        static void UploadLighting(const Ref<Shader>& shader, const Math::Vec3& cameraPosition);

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
        static Math::Mat4 s_ViewProjection;
        static Math::Mat4 s_View;
        static Math::Mat4 s_Projection;
        static Math::Vec3 s_CameraPosition;
        static Math::Vec3 s_CameraForward;
        static Math::Mat4 s_DirectionalLightViewProjection;
        static bool s_DirectionalShadowEnabled;
        static bool s_DirectionalShadowPassActive;
    };
}
