#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace HachimiEngine
{
    struct EditorPanelVisibility
    {
        bool Hierarchy = true;
        bool Inspector = true;
        bool ContentBrowser = true;
        bool Viewport = true;
        bool Game = true;
        bool Console = false;
        bool PackageInspector = false;
        bool SceneSettings = false;
        bool operator==(const EditorPanelVisibility&) const = default;
    };

    // Personal UI state only; project and scene settings keep their existing serializers.
    struct EditorSettings
    {
        std::string Layout;
        EditorPanelVisibility Panels;
        bool Dirty = false;

        void Reset() { Layout.clear(); Panels = {}; Dirty = true; }
        std::string Serialize() const;
        static bool Deserialize(std::string_view text, EditorSettings& out);
        bool Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path) const;
        static std::filesystem::path GetPath();
    };
}
