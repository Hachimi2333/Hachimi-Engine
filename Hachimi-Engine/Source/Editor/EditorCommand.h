#pragma once

#include "Core/Base.h"

#include <string_view>

namespace HachimiEngine
{
    class Scene;

    // One undoable edit.
    //
    // A command owns everything it needs to both apply and revert itself, which is why the
    // transform command carries copies of the before and after values rather than a pointer into
    // the scene: the scene is free to change underneath the history without invalidating it.
    //
    // Commands live in the engine rather than the editor so they can be exercised headlessly. The
    // editor owns the history and the keyboard shortcuts; nothing here knows about ImGui.
    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;

        // A short label for the undo entry, e.g. "Move Entity".
        virtual std::string_view GetName() const = 0;

        // First execution and redo.
        virtual void Apply(Scene& scene) = 0;
        virtual void Revert(Scene& scene) = 0;

        // Folds a follow-up edit of the same kind into this one and returns true, so a gizmo drag
        // or a text field becomes a single undo step instead of one per frame or per keystroke.
        //
        // The incoming command is only inspected when it is the same kind of edit on the same
        // target; implementations return false for anything else, which makes the caller push it
        // as a new entry.
        virtual bool TryMerge(const EditorCommand& next) { (void)next; return false; }

        // Called once when the command reaches the history and the scene is in its final state.
        //
        // Commands that record the scene rather than a value - entity creation, for instance - use
        // this to capture what the edit turned into, because at construction time the user has not
        // finished editing yet.
        virtual void Commit(Scene& scene) { (void)scene; }
    };
}
