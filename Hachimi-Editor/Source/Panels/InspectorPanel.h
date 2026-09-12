#pragma once

#include "Core/Base.h"
#include "Scene/Entity.h"
#include "UI/AssetPickerPopup.h"

namespace HachimiEngine
{
    struct EditorContext;

    // Property editor for the currently selected entity.
    //
    // The panel owns the frame and the generic parts - the entity tag, the component list, the
    // add menu - and delegates each component's rows to its drawer, looked up through
    // InspectorRegistry. It therefore has no branch per component type.
    class InspectorPanel
    {
    public:
        void Draw(EditorContext& context);

    private:
        void DrawAddComponentMenu(Entity entity);

    private:
        int m_PendingAssetPickerSlot = -1;
        AssetPickerPopup m_AssetPicker;
    };
}
