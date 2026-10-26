#include "Renderer/PhysicsDebugDraw.h"

#include <algorithm>

namespace HachimiEngine
{
    namespace
    {
        constexpr float ByteToUnit = 1.0f / 255.0f;

        // Box3D's smallest point is a quarter of its largest (5 against 20), and this keeps that
        // ratio: a contact point reads as a few centimetres at the scale a scene is authored in.
        constexpr float PointExtentScale = 0.01f;
    }

    Math::Vec4 DecodePhysicsDebugColor(uint32_t packedColor, float alpha)
    {
        const uint32_t rgb = packedColor & 0x00FFFFFFu;

        return {
            static_cast<float>((rgb >> 16) & 0xFFu) * ByteToUnit,
            static_cast<float>((rgb >> 8) & 0xFFu) * ByteToUnit,
            static_cast<float>(rgb & 0xFFu) * ByteToUnit,
            alpha
        };
    }

    float PhysicsDebugPointExtent(float pointSize)
    {
        return std::max(pointSize, 0.0f) * PointExtentScale;
    }
}
