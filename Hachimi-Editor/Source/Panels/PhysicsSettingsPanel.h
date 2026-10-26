#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    struct EditorContext;

    // Scene-level physics settings, and what the running simulation is doing with them.
    //
    // Every edit goes through the undo history while the scene is being edited, and writes straight
    // into the running scene while it is playing: a play session's copy is thrown away when it stops,
    // so there is nothing for the history to revert to, and the point of editing it is to feel the
    // change immediately.
    //
    // The values land on the world on the next fixed step, which is what makes them live: gravity,
    // the two enable flags, the worker count and the fixed step are all read from the scene's settings
    // while the scene runs rather than only when a world is created.
    class PhysicsSettingsPanel
    {
    public:
        void Draw(EditorContext& context);
    };
}
