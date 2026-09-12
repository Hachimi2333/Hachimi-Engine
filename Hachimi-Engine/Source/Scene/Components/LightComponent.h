#pragma once

#include "Scene/ComponentRegistry.h"
#include "Math/Math.h"

namespace HachimiEngine
{
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

    ComponentDescriptor MakeLightComponentDescriptor();
}
