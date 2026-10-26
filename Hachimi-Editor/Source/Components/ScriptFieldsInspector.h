#pragma once

#include "Scripting/ScriptFields.h"

#include <vector>

namespace HachimiEngine
{
    // Draws one script slot's field table and edits it in place.
    //
    // The table is the authoring surface for a script's values: the names here are the keys the
    // script indexes self.fields with. `editRuntimeValues` is true while the editor is playing,
    // where the inspector shows the runtime scene and therefore has to edit the value the script
    // reads; outside Play the design-time value is edited, because that is what the scene stores.
    //
    // Returns true when something changed, so the caller can record one undo entry for the slot. A
    // changed row is applied to `fields` immediately either way, which is what makes the controls
    // behave like the rest of the inspector.
    bool DrawScriptFieldsInspector(std::vector<ScriptField>& fields, bool editRuntimeValues);
}
