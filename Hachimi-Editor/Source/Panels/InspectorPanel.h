#pragma once

#include "Core/Base.h"
#include "Scene/Entity.h"

namespace HachimiEngine
{
    struct EditorContext;

    // Property editor for the currently selected entity.
    class InspectorPanel
    {
    public:
        void Draw(EditorContext& context);

    private:
        void DrawAddComponentMenu(EditorContext& context, Entity entity);
        void DrawTransform(Entity entity);
        void DrawMesh(Entity entity);
        void DrawCamera(Entity entity);
        void DrawLight(Entity entity);
    };
}
