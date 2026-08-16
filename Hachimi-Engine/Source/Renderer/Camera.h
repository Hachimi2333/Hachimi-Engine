#pragma once

#include "Core/Base.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    // Perspective camera used by the scene renderer.
    class Camera
    {
    public:
        Camera() = default;
        Camera(const Math::Mat4& projection);

        virtual ~Camera() = default;

        const Math::Mat4& GetProjection() const { return m_Projection; }
        void SetProjection(const Math::Mat4& projection) { m_Projection = projection; }

    protected:
        Math::Mat4 m_Projection { 1.0f };
    };
}
