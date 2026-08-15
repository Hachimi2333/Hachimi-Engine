#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <array>

namespace HachimiEngine
{
    struct DirectionalLight
    {
        glm::vec3 Direction { -0.5f, -1.0f, -0.3f };
        glm::vec3 Color { 1.0f, 0.98f, 0.95f };
        float Intensity = 1.4f;
    };

    struct PointLight
    {
        glm::vec3 Position { 3.0f, 4.0f, 2.0f };
        glm::vec3 Color { 1.0f, 0.9f, 0.7f };
        float Intensity = 12.0f;
    };

    struct LightingEnvironment
    {
        DirectionalLight Directional;
        std::array<PointLight, 4> PointLights;
        int PointLightCount = 1;
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
        static void EndScene();

        static LightingEnvironment& GetLightingEnvironment() { return s_Lighting; }
        static Ref<Material> GetDefaultMaterial() { return s_DefaultMaterial; }

    private:
        static void UploadLighting(const Ref<Shader>& shader, const glm::vec3& cameraPosition);

    private:
        static Ref<Shader> s_DefaultShader;
        static Ref<Shader> s_GridShader;
        static Ref<Material> s_DefaultMaterial;
        static Ref<Mesh> s_GridMesh;
        static LightingEnvironment s_Lighting;
        static glm::mat4 s_ViewProjection;
        static glm::vec3 s_CameraPosition;
    };
}
