#include "Renderer/Camera.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    Camera::Camera(const Math::Mat4& projection)
        : m_Projection(projection)
    {
    }
}
