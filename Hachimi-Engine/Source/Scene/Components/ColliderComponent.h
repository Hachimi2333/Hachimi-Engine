#pragma once

#include "Scene/ComponentRegistry.h"
#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    struct ColliderComponent
    {
        enum class ColliderShapeType
        {
            Box = 0,
            Sphere = 1,
            Capsule = 2,
            Plane = 3
        };

        ColliderShapeType ShapeType = ColliderShapeType::Box;
        Math::Vec3 HalfExtents { 0.5f, 0.5f, 0.5f }; // Box half sizes; Plane half width/depth.
        float Radius = 0.5f;                          // Sphere radius; Capsule radius.
        float Height = 1.0f;                          // Capsule total height, including both caps.
        Math::Vec3 Offset { 0.0f };
        float Density = 1.0f;
        float Friction = 0.6f;
        float Restitution = 0.0f;
        float RollingResistance = 0.0f;
        bool IsTrigger = false;
        uint64_t CategoryBits = ~0ull;
        uint64_t MaskBits = ~0ull;
    };

    // Adds a collider sized to match the entity's mesh primitive, falling back to a unit box.
    // Used by the collider's own default and by a rigid body, which needs a shape to simulate.
    void AddDefaultCollider(entt::registry& registry, entt::entity entity);

    ComponentDescriptor MakeColliderComponentDescriptor();
}
