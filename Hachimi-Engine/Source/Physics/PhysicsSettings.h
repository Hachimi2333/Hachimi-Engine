#pragma once

#include "Math/Math.h"

namespace HachimiEngine
{
    // Scene-level physics configuration. Box3D defaults use meters/kilograms/seconds
    // and a +Y up gravity vector.
    //
    // Kept apart from PhysicsWorld so a scene can carry the settings without depending on the
    // physics backend: the world that consumes them lives in PhysicsSystem.
    struct PhysicsSettings
    {
        Math::Vec3 Gravity { 0.0f, -10.0f, 0.0f };
        float FixedTimeStep = 1.0f / 60.0f;
        int SubStepCount = 4;
        bool EnableSleep = true;
        bool EnableContinuous = true;
    };
}
