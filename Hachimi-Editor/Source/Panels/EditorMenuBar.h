#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Top-level editor menu bar.
    class EditorMenuBar
    {
    public:
        void Draw(EditorLayer* owner, EditorContext& context);

    private:
        void OpenScene(EditorLayer* owner, EditorContext& context);
        void SaveScene();
        void SaveSceneAs();
        void ImportTexture();
    };
}
