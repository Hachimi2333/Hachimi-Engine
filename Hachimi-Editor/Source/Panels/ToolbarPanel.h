#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Top toolbar hosting gizmo mode buttons and playback controls.
    class ToolbarPanel
    {
    public:
        void Draw(EditorLayer* owner, EditorContext& context);
    };
}
