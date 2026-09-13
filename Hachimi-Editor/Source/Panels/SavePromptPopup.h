#pragma once

#include "Core/Base.h"

#include <string>

namespace HachimiEngine
{
    // Modal that stands between a destructive action and unsaved edits.
    //
    // It asks one question - save, discard or cancel - and reports the answer to the layer that
    // owns the pending action. Keeping it a single modal rather than a prompt per call site is what
    // makes "closing the editor", "switching scene" and "returning to the hub" all behave the same.
    class SavePromptPopup
    {
    public:
        enum class Choice
        {
            None = 0,
            Save,
            Discard,
            Cancel
        };

        // Opens the prompt. actionLabel names what the user tried to do, e.g. "Close the editor".
        void Open(const std::string& actionLabel) { m_Open = true; m_ActionLabel = actionLabel; }

        bool IsOpen() const { return m_Open; }

        // Draws the modal and returns the choice made this frame, or None while it is still open.
        Choice Draw();

    private:
        bool m_Open = false;
        std::string m_ActionLabel;
    };
}
