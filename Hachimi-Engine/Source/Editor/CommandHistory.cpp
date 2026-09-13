#include "Editor/CommandHistory.h"

#include "Scene/Scene.h"

#include <utility>

namespace HachimiEngine
{
    CommandHistory::CommandHistory(Scene& scene)
        : m_Scene(&scene)
    {
    }

    void CommandHistory::Execute(Scope<EditorCommand> command)
    {
        if (command == nullptr || m_Scene == nullptr)
        {
            return;
        }

        command->Apply(*m_Scene);
        command->Commit(*m_Scene);
        m_Scene->MarkDirty();

        m_RedoStack.clear();
        m_UndoStack.push_back(std::move(command));
        TrimToCapacity();
    }

    void CommandHistory::ExecuteMerged(Scope<EditorCommand> command)
    {
        if (command == nullptr || m_Scene == nullptr)
        {
            return;
        }

        // The edit is applied first either way: merging only decides how many history entries it
        // costs, never whether the scene changed.
        command->Apply(*m_Scene);

        // A mergeable edit with nothing to merge into, or one that does not match the entry on top,
        // is an ordinary new entry: the check lives inside the command so the rule about what
        // counts as "the same edit" stays next to the data it needs.
        if (!m_RedoStack.empty() || m_UndoStack.empty())
        {
            command->Commit(*m_Scene);
            m_Scene->MarkDirty();

            m_UndoStack.push_back(std::move(command));
            TrimToCapacity();
            return;
        }

        EditorCommand& top = *m_UndoStack.back();
        if (!top.TryMerge(*command))
        {
            command->Commit(*m_Scene);
            m_Scene->MarkDirty();

            m_RedoStack.clear();
            m_UndoStack.push_back(std::move(command));
            TrimToCapacity();
            return;
        }

        // The incoming command only widened the entry on top, so it is dropped rather than pushed.
        top.Commit(*m_Scene);
        m_Scene->MarkDirty();
    }

    bool CommandHistory::Undo()
    {
        if (m_UndoStack.empty() || m_Scene == nullptr)
        {
            return false;
        }

        Scope<EditorCommand> command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        command->Revert(*m_Scene);
        m_Scene->MarkDirty();

        m_RedoStack.push_back(std::move(command));
        return true;
    }

    bool CommandHistory::Redo()
    {
        if (m_RedoStack.empty() || m_Scene == nullptr)
        {
            return false;
        }

        Scope<EditorCommand> command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        command->Apply(*m_Scene);
        m_Scene->MarkDirty();

        m_UndoStack.push_back(std::move(command));
        return true;
    }

    void CommandHistory::SetCapacity(size_t capacity)
    {
        m_Capacity = capacity == 0 ? 1 : capacity;
        TrimToCapacity();
    }

    void CommandHistory::TrimToCapacity()
    {
        while (m_UndoStack.size() > m_Capacity)
        {
            // std::deque has no pop_front on an empty check, and the front is the oldest entry.
            m_UndoStack.pop_front();
        }
    }

    const std::string& CommandHistory::GetUndoName() const
    {
        if (m_UndoStack.empty() || m_UndoStack.back() == nullptr)
        {
            return m_EmptyName;
        }

        // The stack is stable between mutations, so referring to the command's own label by its
        // string_view costs nothing and stays valid as long as the entry does.
        static thread_local std::string name;
        name = std::string(m_UndoStack.back()->GetName());
        return name;
    }

    const std::string& CommandHistory::GetRedoName() const
    {
        if (m_RedoStack.empty() || m_RedoStack.back() == nullptr)
        {
            return m_EmptyName;
        }

        static thread_local std::string name;
        name = std::string(m_RedoStack.back()->GetName());
        return name;
    }

    void CommandHistory::Clear()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }
}
