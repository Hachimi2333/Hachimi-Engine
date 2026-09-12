#pragma once

namespace HachimiEngine
{
    class DebugDraw;
    struct EditorContext;

    // Draws editor-only indicator lines for the currently selected entity.
    // Camera entities show their view frustum; lights show range/direction visuals.
    // The caller owns the debug renderer and calls this while its framebuffer is bound.
    void DrawSelectionIndicators(EditorContext& context, DebugDraw& debugDraw);
}
