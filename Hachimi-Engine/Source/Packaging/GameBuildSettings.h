#pragma once

#include "Core/Base.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace HachimiEngine
{
    // Platforms supported by the game export pipeline. Only Windows is
    // implemented in the current phase; the remaining enum values reserve
    // slots for future platforms without changing the settings format.
    enum class BuildTargetPlatform : uint8_t
    {
        Windows = 0,
        macOS = 1,
        Linux = 2
    };

    // Export options stored per target platform inside the project file.
    struct PlatformBuildSettings
    {
        std::string ProductName;
        // Scene path relative to the project root, for example
        // "Assets/Scenes/Default.hscene".
        std::filesystem::path StartScene;
        uint32_t WindowWidth = 1600;
        uint32_t WindowHeight = 900;
        bool VSync = true;
    };

    // Per-platform export settings owned by a Project.
    struct GameBuildSettings
    {
        static constexpr const char* WindowsPlatformName = "Windows";
        static constexpr const char* MacOSPlatformName = "macOS";
        static constexpr const char* LinuxPlatformName = "Linux";

        std::unordered_map<std::string, PlatformBuildSettings> Platforms;

        PlatformBuildSettings& GetOrCreateWindowsSettings();
        const PlatformBuildSettings* GetWindowsSettings() const;
    };
}
