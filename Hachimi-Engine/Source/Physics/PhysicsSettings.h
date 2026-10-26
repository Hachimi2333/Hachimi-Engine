#pragma once

#include "Math/Math.h"

#include <cstdint>

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

        // Whether solid colliders report begin/end/hit contact events. Box3D keeps a contact's
        // begin-touch flag for the shapes that ask for it, so a scene that never listens for
        // collisions does not pay for the event bookkeeping. Trigger volumes are unaffected: a
        // sensor without overlap events is not a sensor, so IsTrigger implies its events.
        bool EnableContactEvents = true;

        // Workers the simulation may fork onto the engine's thread pool. 0 picks the JobSystem's own
        // worker count, 1 keeps the whole step on the calling thread, and a machine without a running
        // pool simulates single-threaded whatever this says. Defaulted to 1 so a scene keeps the
        // single-threaded behaviour it had before threading existed until someone asks for more.
        uint32_t WorkerCount = 1;
    };
}
