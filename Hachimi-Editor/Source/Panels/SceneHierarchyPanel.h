#pragma once

#include "Core/Base.h"
#include "Scene/Entity.h"

namespace HachimiEngine
{
    struct EditorContext;

    // Hierarchy tree of the active scene with entity creation context menu.
    class SceneHierarchyPanel
    {
    public:
        void Draw(EditorContext& context);

    private:
        void DrawEntityNode(EditorContext& context, Entity entity);
        void DrawEntityContextMenu(EditorContext& context, Entity entity);
        void DrawCreateMenu(EditorContext& context);
    };
}
