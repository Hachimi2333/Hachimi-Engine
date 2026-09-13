#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Top-level editor menu bar.
    //
    // The menu only names the action: the layer owns what the action does, which is what lets the
    // menu, the keyboard shortcut and the content browser's context menu all go through the same
    // save prompt and the same undo history.
    class EditorMenuBar
    {
    public:
        void Draw(EditorLayer* owner, EditorContext& context);

    private:
        void ImportAsset(EditorLayer& layer, EditorContext& context);
    };
}
