// GameBuildSettings: the per-platform export options stored inside a project file.

#include <doctest/doctest.h>

#include "Packaging/GameBuildSettings.h"

#include <string>

using namespace HachimiEngine;

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("GameBuildSettings creates the Windows entry on demand")
{
    GameBuildSettings settings;
    CHECK(settings.GetWindowsSettings() == nullptr);
    CHECK(settings.Platforms.empty());

    PlatformBuildSettings& windows = settings.GetOrCreateWindowsSettings();
    CHECK(settings.Platforms.size() == 1);
    REQUIRE(settings.GetWindowsSettings() == &windows);

    // Defaults an exporter relies on when a project file leaves them out.
    CHECK(windows.ProductName.empty());
    CHECK(windows.StartScene.empty());
    CHECK(windows.WindowWidth == 1600u);
    CHECK(windows.WindowHeight == 900u);
    CHECK(windows.VSync == true);

    // Asking again returns the same entry instead of replacing it.
    windows.ProductName = "Kept";
    PlatformBuildSettings& again = settings.GetOrCreateWindowsSettings();
    CHECK(again.ProductName == "Kept");
    CHECK(&again == settings.GetWindowsSettings());
    CHECK(settings.Platforms.size() == 1);

    settings.Platforms.clear();
    CHECK(settings.GetWindowsSettings() == nullptr);
}

TEST_CASE("the platform name constants are the serialized keys")
{
    CHECK(std::string(GameBuildSettings::WindowsPlatformName) == "Windows");
    CHECK(std::string(GameBuildSettings::MacOSPlatformName) == "macOS");
    CHECK(std::string(GameBuildSettings::LinuxPlatformName) == "Linux");
}

TEST_SUITE_END();
