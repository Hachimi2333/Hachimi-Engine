#pragma once

#include "Core/UUID.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

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
        Math::Vec3 Position { 0.0f };
        Math::Vec3 Rotation { 0.0f }; // Euler angles in degrees.
        Math::Vec3 Scale { 1.0f };

        Math::Mat4 GetTransform() const
        {
            const Math::Quat rotation = Math::Quat(Math::Radians(Rotation));
            return Math::Translate(Math::Mat4(1.0f), Position)
                * Math::ToMat4(rotation)
                * Math::Scale(Math::Mat4(1.0f), Scale);
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
        Math::Vec4 MaterialColor { 0.8f, 0.8f, 0.82f, 1.0f };
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
        Math::Vec3 Color { 1.0f, 1.0f, 1.0f };
        float Intensity = 10.0f;
        float Range = 12.0f;
        bool CastsShadows = true;
        float ShadowBias = 0.0005f;
    };
}
