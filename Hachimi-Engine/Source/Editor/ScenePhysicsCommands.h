#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Editor/EditorCommand.h"
#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    // Commands the editor issues while editing a scene's physics settings.
    //
    // They mirror the per-entity commands in SceneCommands, but the value they carry lives on the
    // Scene rather than on a component, so there is no entity to find when they are applied: a
    // settings command records both values and writes one or the other.
    //
    // Every factory takes the value already in the scene and the value the user just picked, which is
    // what the caller has at hand; nothing reads the scene at construction time.
    namespace ScenePhysicsCommands
    {
        Scope<EditorCommand> MakeSetGravity(const Math::Vec3& before, const Math::Vec3& after);
        Scope<EditorCommand> MakeSetFixedTimeStep(float before, float after);
        Scope<EditorCommand> MakeSetSubStepCount(int before, int after);
        Scope<EditorCommand> MakeSetEnableSleep(bool before, bool after);
        Scope<EditorCommand> MakeSetEnableContinuous(bool before, bool after);
        Scope<EditorCommand> MakeSetEnableContactEvents(bool before, bool after);
        Scope<EditorCommand> MakeSetWorkerCount(uint32_t before, uint32_t after);
    }
}
