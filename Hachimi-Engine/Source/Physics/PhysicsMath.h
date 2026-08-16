#pragma once

// Conversion helpers between HachimiEngine::Math and the Box3D public math types.
// Box3D headers stay out of the ECS and editor-facing headers.

#include <box3d/box3d.h>

#include "Math/Math.h"

namespace HachimiEngine
{
    inline b3Vec3 ToBox3D(const Math::Vec3& value)
    {
        return { value.x, value.y, value.z };
    }

    inline Math::Vec3 ToMath(const b3Vec3& value)
    {
        return { value.x, value.y, value.z };
    }

    inline b3Quat ToBox3D(const Math::Quat& value)
    {
        return { { value.x, value.y, value.z }, value.w };
    }

    inline Math::Quat ToMath(const b3Quat& value)
    {
        return { value.s, value.v.x, value.v.y, value.v.z };
    }

    inline Math::Mat4 ToMath(const b3Transform& transform)
    {
        Math::Mat4 matrix = Math::ToMat4(ToMath(transform.q));
        matrix[3] = Math::Vec4(transform.p.x, transform.p.y, transform.p.z, 1.0f);
        return matrix;
    }

    inline b3Transform ToBox3D(const Math::Vec3& position, const Math::Quat& rotation)
    {
        return { ToBox3D(position), ToBox3D(rotation) };
    }
}
