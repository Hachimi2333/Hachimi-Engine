#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // OS-specific path helpers used by the project hub.
    class PlatformUtils
    {
    public:
        static std::filesystem::path GetUserDocumentsDirectory();
        static std::filesystem::path GetApplicationDataDirectory();
        static std::filesystem::path GetDefaultProjectsDirectory();
        static void OpenPathInExplorer(const std::filesystem::path& path);
    };
}
