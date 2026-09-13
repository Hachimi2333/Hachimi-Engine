#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Editor/EditorCommand.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Entity.h"

#include <imgui.h>

#include <functional>

namespace HachimiEngine
{
    // Shared ImGui building blocks for the component drawers.
    //
    // They live outside InspectorPanel so that a drawer is a small function next to the
    // component it edits, instead of one more branch inside the panel.

    class CommandHistory;
    struct InspectorDrawContext;

    // Small square remove button with a red hover state. Keep the ID stack owned by the caller.
    bool DrawRemoveButton(const char* tooltip);

    // Two-column table used by every property section.
    bool BeginInspectorTable(const char* id);

    // Moves to the next property row and makes the control fill the whole control column.
    void BeginInspectorProperty(const char* label);

    // Same as BeginInspectorProperty, but lets the caller size the control manually.
    void BeginInspectorPropertyLabel(const char* label);

    // Full available inspector width, for buttons that span the panel.
    ImVec2 GetFullWidthButtonSize();

    // Collapsible header with a right-aligned remove button.
    //
    // Returns false when the section is collapsed, or when the component was just removed;
    // `removed` distinguishes the two. Required components get no remove button, because
    // removing them is never valid.
    bool DrawComponentHeader(Entity entity, const ComponentDescriptor& descriptor, bool defaultOpen, bool& removed);

    // Same, but records the removal as an undoable edit.
    //
    // A component that cannot be restored is a component the user will not dare to remove, so the
    // drawer passes the entity here rather than calling the descriptor directly.
    bool DrawComponentHeaderUndoable(Entity entity, const ComponentDescriptor& descriptor, bool defaultOpen,
                                     bool& removed, CommandHistory* history);

    // Records "this field changed from beforeValue to afterValue" as one undo step.
    //
    // This is the glue between an ImGui widget and the command system: the widget has already
    // written the new value into the component, so the only thing left is to remember what it
    // replaced. Call it from the branch where the widget reported an edit, and use
    // DrawStringUndoable for text fields, which have to capture the old text before the user types.
    void RecordEdit(CommandHistory* history, Scope<EditorCommand> command);

    // Draws an InputText whose edits are undoable and merge while the user keeps typing.
    // Returns true when the text changed this frame.
    bool DrawStringUndoable(const char* id, Entity entity, std::string& value, CommandHistory* history,
                            int flags = 0);
}
