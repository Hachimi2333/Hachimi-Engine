#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Editor/EditorCommand.h"

#include <cstddef>
#include <deque>
#include <string>

namespace HachimiEngine
{
    class Scene;

    // Undo/redo stack for one scene.
    //
    // The history holds only commands, never scene snapshots: reverting is the command's job, so
    // a hundred edits cost a hundred small records instead of a hundred copies of the scene.
    // Every mutation that goes through here also marks the scene dirty, which is what makes the
    // save prompt reliable - there is one write path to remember, not one per panel.
    class CommandHistory
    {
    public:
        // Deep histories are a memory leak with a friendly name; the oldest entry is dropped once
        // the cap is reached, which is standard behaviour for scene editors.
        static constexpr size_t DefaultCapacity = 128;

        explicit CommandHistory(Scene& scene);

        // Applies the command, records it and drops the redo stack.
        void Execute(Scope<EditorCommand> command);
        // Applies the command like Execute, but first offers it to the top of the undo stack for
        // merging. Use this for continuous edits: a gizmo drag, a slider, a text field.
        void ExecuteMerged(Scope<EditorCommand> command);

        bool Undo();
        bool Redo();

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }
        size_t GetUndoCount() const { return m_UndoStack.size(); }
        size_t GetRedoCount() const { return m_RedoStack.size(); }
        size_t GetCapacity() const { return m_Capacity; }
        void SetCapacity(size_t capacity);

        // Label of the edit Undo would revert, or an empty string.
        const std::string& GetUndoName() const;
        const std::string& GetRedoName() const;

        // Drops both stacks. Called when the edited scene changes, never on its own.
        void Clear();

        // The scene this history edits. The editor compares it when the edit target changes.
        Scene& GetScene() const { return *m_Scene; }

    private:
        void TrimToCapacity();

        Scene* m_Scene = nullptr;
        std::deque<Scope<EditorCommand>> m_UndoStack;
        std::deque<Scope<EditorCommand>> m_RedoStack;
        size_t m_Capacity = DefaultCapacity;
        std::string m_EmptyName;
    };
}
