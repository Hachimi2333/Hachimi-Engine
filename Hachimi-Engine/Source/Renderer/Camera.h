#pragma once

#include "Core/Base.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    // Perspective camera used by the scene renderer.
    class Camera
    {
    public:
        Camera() = default;
        Camera(const glm::mat4& projection);

        virtual ~Camera() = default;

        const glm::mat4& GetProjection() const { return m_Projection; }
        void SetProjection(const glm::mat4& projection) { m_Projection = projection; }

    protected:
        glm::mat4 m_Projection { 1.0f };
    };
}
