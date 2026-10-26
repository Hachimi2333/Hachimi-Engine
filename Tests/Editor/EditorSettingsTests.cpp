#include <doctest/doctest.h>

#include "Editor/EditorSettings.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE("Editor")
{
    TEST_CASE("personal layout and panel state round trip through YAML and atomic replacement")
    {
        const auto directory = Workspace().PrepareDirectory("editor_settings");
        const auto path = directory / "EditorSettings.yaml";
        EditorSettings settings;
        settings.Layout = "[Window][Inspector]\nPos=20,30\n\n[Docking][Data]\n";
        settings.Panels.Console = true;
        settings.Panels.Game = false;
        REQUIRE(settings.Save(path));
        EditorSettings loaded;
        REQUIRE(loaded.Load(path));
        CHECK(loaded.Layout == settings.Layout);
        CHECK(loaded.Panels == settings.Panels);
        loaded.Panels.SceneSettings = true;
        REQUIRE(loaded.Save(path));
        REQUIRE(settings.Load(path));
        CHECK(settings.Panels.SceneSettings);
        CHECK_FALSE(FileSystem::Exists(directory / "imgui.ini"));
        CHECK_FALSE(FileSystem::Exists(directory / "EditorSettings.yaml.tmp"));
        settings.Reset();
        CHECK(settings.Layout.empty());
        CHECK(settings.Panels == EditorPanelVisibility {});
        CHECK(settings.Dirty);
    }

    TEST_CASE("malformed or unsupported personal settings return clean defaults")
    {
        for (const char* text : {"[invalid", "Version: 99", "Version: 1\nPanels: []",
                 "Version: 1\nPanels: {Console: broken}", "Version: 1\nLayout: corrupt"})
        {
            EditorSettings settings;
            settings.Panels.Console = true;
            CHECK_FALSE(EditorSettings::Deserialize(text, settings));
            CHECK(settings.Layout.empty());
            CHECK(settings.Panels == EditorPanelVisibility {});
        }
        EditorSettings settings;
        CHECK(EditorSettings::Deserialize("Version: 1", settings));
        CHECK_FALSE(settings.Save({}));
        CHECK_FALSE(settings.Save("EditorSettings.yaml"));
    }
}
