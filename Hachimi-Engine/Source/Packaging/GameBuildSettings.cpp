#include "Packaging/GameBuildSettings.h"

namespace HachimiEngine
{
    PlatformBuildSettings& GameBuildSettings::GetOrCreateWindowsSettings()
    {
        return Platforms[WindowsPlatformName];
    }

    const PlatformBuildSettings* GameBuildSettings::GetWindowsSettings() const
    {
        const auto it = Platforms.find(WindowsPlatformName);
        return it == Platforms.end() ? nullptr : &it->second;
    }
}
