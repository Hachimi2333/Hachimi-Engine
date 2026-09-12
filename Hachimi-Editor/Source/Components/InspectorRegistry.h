#pragma once

#include "Scene/Entity.h"

namespace HachimiEngine
{
    struct EditorContext;
    class AssetPickerPopup;

    // Everything a component drawer may touch beyond the entity itself.
    struct InspectorDrawContext
    {
        EditorContext& Context;

        // Asset picker owned by the panel. Drawers never own UI state, so closing or resetting
        // the picker stays the panel's business.
        AssetPickerPopup& AssetPicker;

        // Slot inside the component that an open asset picker is choosing a path for, or -1.
        int& PendingAssetPickerSlot;
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
