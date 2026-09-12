#pragma once

#include "Core/Base.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Entity.h"

#include <imgui.h>

namespace HachimiEngine
{
    // Shared ImGui building blocks for the component drawers.
    //
    // They live outside InspectorPanel so that a drawer is a small function next to the
    // component it edits, instead of one more branch inside the panel.

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
}
