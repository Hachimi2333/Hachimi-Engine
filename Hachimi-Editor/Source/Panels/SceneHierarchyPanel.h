#pragma once

#include "Core/Base.h"
#include "Scene/Entity.h"
#include "Renderer/MeshFactory.h"

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

        // Creating a mesh entity means the same four lines everywhere a primitive is offered.
        Entity CreatePrimitive(EditorContext& context, const char* name, PrimitiveMeshType primitive);
    };
}
