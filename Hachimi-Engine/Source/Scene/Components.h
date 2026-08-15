#pragma once

#include "Core/UUID.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>

namespace HachimiEngine
{
    struct IDComponent
    {
        UUID ID;
    };

    struct TagComponent
    {
        std::string Tag = "Entity";
    };

    struct TransformComponent
    {
        glm::vec3 Position { 0.0f };
        glm::vec3 Rotation { 0.0f }; // Euler angles in degrees.
        glm::vec3 Scale { 1.0f };

        glm::mat4 GetTransform() const
        {
            const glm::quat rotation = glm::quat(glm::radians(Rotation));
            return glm::translate(glm::mat4(1.0f), Position)
                * glm::toMat4(rotation)
                * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    struct RelationshipComponent
    {
        UUID Parent = UUID::Invalid();
        std::vector<UUID> Children;
    };

    struct MeshComponent
    {
        Ref<Mesh> Mesh;
        PrimitiveMeshType PrimitiveType = PrimitiveMeshType::Cube;
        Ref<Material> MaterialOverride;
        glm::vec4 MaterialColor { 0.8f, 0.8f, 0.82f, 1.0f };
        float Roughness = 0.6f;
        float Metallic = 0.05f;
        bool Visible = true;
    };

    struct CameraComponent
    {
        bool Primary = false;
        float FieldOfView = 45.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
    };

    struct LightComponent
    {
        enum class LightType
        {
            Directional = 0,
            Point = 1
        };

        LightType Type = LightType::Point;
        glm::vec3 Color { 1.0f, 1.0f, 1.0f };
        float Intensity = 10.0f;
    };
}
