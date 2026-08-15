#pragma once

namespace HachimiEngine
{
    // Scene-level settings that affect the shared environment rendering.
    struct EnvironmentSettings
    {
        bool ShowSkybox = true;
        float Exposure = 1.0f;
        float EnvironmentIntensity = 1.0f;
    };
}
