#pragma once

#include "Scene/Entity.h"
#include "UI/AssetField.h"

#include <entt/entt.hpp>

namespace HachimiEngine
{
    struct EditorContext;

    // Everything a component drawer may touch beyond the entity itself.
    //
    // The drawer owns no UI state: the picker, the pending field and the assigned slot all live
    // here, so opening or cancelling a modal stays the panel's business and a drawer is a plain
    // function with no lifetime.
    struct InspectorDrawContext
    {
        EditorContext& Context;

        AssetFieldState& AssetFields;

        // Slot the field being drawn occupies, set by the drawer before it calls DrawAssetField.
        uint32_t NextAssetFieldSlot = 0;

        // Asset the panel resolved from an open picker this frame, for drawers whose field is not
        // the generic asset slot - a script inside a list, for example.
        AssetHandle AssignedAsset;

        // True when AssignedAsset is set, so a drawer knows to consume it.
        bool HasAssignedAsset = false;
    };

    using ComponentDrawFn = void (*)(Entity entity, InspectorDrawContext& context);

    // One drawer per component type, keyed by the type id in its ComponentDescriptor.
    //
    // The panel walks ComponentRegistry and asks this table for the matching drawer, so adding a
    // component adds one line here instead of a branch in the panel's dispatch.
    class InspectorRegistry
    {
    public:
        static void Register(entt::id_type typeId, ComponentDrawFn draw);

        // Returns nullptr for a component with no drawer; the panel then shows the raw
        // "no editor yet" note rather than failing.
        static ComponentDrawFn Find(entt::id_type typeId);

        // Registers the built-in drawers exactly once.
        static void EnsureBuiltinDrawersRegistered();

    private:
        static void RegisterBuiltinDrawers();
    };
}
