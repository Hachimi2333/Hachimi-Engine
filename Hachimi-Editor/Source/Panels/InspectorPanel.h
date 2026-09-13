#pragma once

#include "Asset/AssetHandle.h"
#include "Asset/MaterialAsset.h"
#include "Core/Base.h"
#include "Scene/Entity.h"
#include "UI/AssetField.h"

namespace HachimiEngine
{
    class AssetDatabase;
    struct EditorContext;

    // Property editor for the selected entity, or for the selected asset.
    //
    // The panel owns the frame and the generic parts - the entity tag, the component list, the add
    // menu, the material asset editor - and delegates each component's rows to its drawer, looked up
    // through InspectorRegistry. It therefore has no branch per component type.
    class InspectorPanel
    {
    public:
        void Draw(EditorContext& context);

    private:
        void DrawEntityInspector(EditorContext& context, Entity entity);
        void DrawAddComponentMenu(EditorContext& context, Entity entity);

        void DrawAssetInspector(EditorContext& context);
        // Reads the material document, draws its rows and writes it back when the user asks.
        void DrawMaterialAssetEditor(EditorContext& context);
        void DrawTextureAssetEditor(EditorContext& context);
        void DrawMaterialTextureRow(EditorContext& context, MaterialAsset& edited);

        // Opens the picker for the material document's texture field.
        void OpenMaterialTexturePicker(EditorContext& context);
        // Takes the picker's answer for the material document, if that is what was open.
        bool ConsumeMaterialTextureSelection(EditorContext& context);

        // Applies the result of an open asset picker to whichever field requested it.
        void ApplyPendingAssetSelection(EditorContext& context);
        // Rebinds the material edit buffer when the inspected asset changes.
        void EnsureMaterialBuffer(AssetDatabase& assets, AssetHandle handle);

    private:
        AssetFieldState m_AssetFields;

        // The document being edited. A material is a file, so the panel keeps the buffer across
        // frames and only touches the file when the user applies or reverts.
        AssetHandle m_MaterialBufferFor;
        MaterialAsset m_MaterialBuffer;
        bool m_MaterialBufferValid = false;
        bool m_MaterialChanged = false;
    };
}
