#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace HachimiEngine
{
    class AssetDatabase;
    class TextureCache;

    // Shared large-icon asset grid used by the Content Browser and the inspector asset picker.
    //
    // The grid only draws items and resolves thumbnails; every action a file has - open, rename,
    // delete, drag onto a folder - is reported to the caller, so the same widget serves a read-only
    // picker and a browser with a context menu.
    class AssetBrowserGrid
    {
    public:
        // ImGui drag-and-drop payload carrying the absolute path of a Content Browser file.
        static constexpr const char* FilePayload = "CONTENT_BROWSER_FILE";

        struct Callbacks
        {
            // Called when an item is double-clicked.
            std::function<void(const std::filesystem::path& path)> OnActivate;
            // Called when a file is dropped onto a directory item. The empty directory signals a
            // drop on the grid itself, which the Content Browser resolves to the folder it shows.
            std::function<void(const std::filesystem::path& droppedPath, const std::filesystem::path& targetDirectory)> OnDrop;
            // Called when the selection moves to a different item.
            std::function<void(const std::filesystem::path& path)> OnSelectionChanged;
            // Fills the context menu for an item, including its opening item.
            std::function<void(const std::filesystem::path& path)> DrawContextMenu;

            // Services used to resolve a texture thumbnail. Either may be null, in which case every
            // file draws its glyph instead of a preview.
            const AssetDatabase* Database = nullptr;
            TextureCache* Textures = nullptr;
        };

        // Draws directories first, then files. selectedPath is updated on single click and the
        // callbacks report activation and drops. Callers own every side effect.
        static void Draw(
            const std::vector<std::filesystem::path>& directories,
            const std::vector<std::filesystem::path>& files,
            std::filesystem::path& selectedPath,
            const Callbacks& callbacks);

        // True when the file is an image this build can decode, so callers can decide whether to
        // offer a thumbnail at all.
        static bool IsImagePath(const std::filesystem::path& path);
    };
}
