#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Keyboard shortcuts the editor listens for.
    //
    // They are handled in one place rather than spread over the panels, so "Ctrl+Z" cannot mean
    // two different things depending on which window has focus, and so every one of them goes
    // through the same undo history and save prompt as the equivalent menu entry.
    namespace EditorShortcuts
    {
        // Processes the frame's shortcuts. Does nothing while the user is typing into a text field,
        // which ImGui reports through WantCaptureKeyboard.
        void Handle(EditorLayer& layer, EditorContext& context);
    }
}
