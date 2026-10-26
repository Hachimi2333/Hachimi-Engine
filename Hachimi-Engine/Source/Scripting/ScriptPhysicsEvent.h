#pragma once

#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    // Backend-independent callback payload, queued in simulation order until script Update.
    struct ScriptPhysicsEvent
    {
        enum class Kind { CollisionEnter, CollisionExit, TriggerEnter, TriggerExit };
        Kind Type;
        uint64_t EntityKeyA = 0;
        uint64_t EntityKeyB = 0;
        Math::Vec3 Point { 0.0f };
        Math::Vec3 Normal { 0.0f };
        float Impulse = 0.0f;
    };
}
