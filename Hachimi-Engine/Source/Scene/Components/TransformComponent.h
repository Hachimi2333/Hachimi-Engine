#pragma once

#include "Scene/ComponentRegistry.h"
#include "Math/Math.h"

namespace HachimiEngine
{
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

    ComponentDescriptor MakeTransformComponentDescriptor();
}
