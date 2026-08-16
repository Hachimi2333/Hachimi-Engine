#pragma once

namespace HachimiEngine
{
    struct EditorContext;

    // Draws editor-only indicator lines for the currently selected entity.
    // Camera entities show their view frustum; lights show range/direction visuals.
    void DrawSelectionIndicators(EditorContext& context);
}
