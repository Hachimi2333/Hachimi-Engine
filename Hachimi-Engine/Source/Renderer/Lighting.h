#pragma once

#include "Core/Base.h"
#include "Math/Math.h"

#include <array>

namespace HachimiEngine
{
    // Lights as the renderer consumes them: a scene extracts its LightComponents into
    // these plain values, and the renderer never looks at the registry.
    struct DirectionalLight
    {
        Math::Vec3 Direction { -0.5f, -1.0f, -0.3f };
        Math::Vec3 Color { 1.0f, 0.98f, 0.95f };
        float Intensity = 1.4f;
        bool CastsShadows = true;
        float ShadowBias = 0.0005f;
    };

    struct PointLight
    {
        Math::Vec3 Position { 3.0f, 4.0f, 2.0f };
        Math::Vec3 Color { 1.0f, 0.9f, 0.7f };
        float Intensity = 12.0f;
        float Range = 12.0f;
    };

    struct LightingEnvironment
    {
        // The scene shader declares the same fixed array, so this is the number of
        // point lights that can be lit at once. Scene::BuildRenderView warns when a
        // scene exceeds it.
        static constexpr size_t MaxPointLights = 4;

        DirectionalLight Directional;
        std::array<PointLight, MaxPointLights> PointLights;
        int PointLightCount = 1;
        Math::Vec3 AmbientColor { 0.08f, 0.08f, 0.10f };
        float AmbientIntensity = 1.0f;
    };
}
