#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <vector>

namespace HachimiEngine
{
    // Shared large-icon asset grid used by the Content Browser and the inspector asset picker.
    class AssetBrowserGrid
    {
    public:
        // ImGui drag-and-drop payload carrying the absolute path of a Content Browser file.
        static constexpr const char* FilePayload = "CONTENT_BROWSER_FILE";

        // Draws directories first, then files. selectedPath is updated on single click and
        // activatedPath is set when an item is double-clicked. Callers must clear activatedPath
        // before calling Draw.
        static void Draw(
            const std::vector<std::filesystem::path>& directories,
            const std::vector<std::filesystem::path>& files,
            std::filesystem::path& selectedPath,
            std::filesystem::path& activatedPath);
    };
}
