#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"
#include "Core/UUID.h"
#include "UI/AssetPickerPopup.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace HachimiEngine
{
    class AssetDatabase;
    class Entity;
    class TextureCache;
    struct EditorContext;
    struct InspectorDrawContext;

    // Which asset field an open picker is choosing for.
    //
    // A field is identified by the entity and a slot number inside it rather than by a pointer,
    // because the picker is a modal: several frames pass between opening it and the choice coming
    // back, and a pointer into the registry would be stale by then. The slot numbers a field by
    // listing order inside the component, so a field added later simply takes the next number.
    struct AssetFieldSlot
    {
        UUID Entity;
        uint32_t Slot = 0;

        bool IsValid() const { return Entity != UUID::Invalid(); }
        void Clear() { Entity = UUID::Invalid(); }
    };

    // Slot numbers a single-reference field can occupy inside an entity.
    //
    // A drawer numbers the fields it draws from zero, so two components can each own slot 0 without
    // colliding: the slot is only ever interpreted by the component that declared it. The script
    // list is the one multi-reference field and reserves a range above the single ones.
    inline constexpr uint32_t AssetFieldMaterialSlot = 0;
    inline constexpr uint32_t AssetFieldScriptSlotBase = 1;

    // State one InspectorPanel passes to the asset field rows it draws.
    struct AssetFieldState
    {
        AssetPickerPopup Picker;
        AssetFieldSlot Pending;
    };

    // Draws one asset reference row: the current asset's name, a browse button, a clear button and
    // a drop target for content-browser drags.
    //
    // Every reference in the editor is drawn through this one widget, so they all accept a drag,
    // all report a missing asset the same way, and all record their change as a single undo step.
    // The row is responsible for declaring its own slot; the caller only says which one it is.
    void DrawAssetField(const char* id, Entity entity, uint32_t slot, AssetHandle& handle, AssetType expectedType,
                        AssetFieldState& state, InspectorDrawContext& context);

    // Assigns a handle to the field a pending choice belongs to. Returns true when the field was
    // found and changed. Callers that own an unusual field shape - a script slot inside a list -
    // use this from their own ApplyAssignedAsset hook.
    bool ApplyAssetToEntity(EditorContext& context, AssetFieldSlot slot, AssetHandle handle);

    // Folder the picker should open on for an asset kind, and the extension it should filter to.
    std::filesystem::path GetPickerRootFor(const AssetDatabase* database, AssetType type);
    std::string GetExtensionFor(AssetType type);
}
