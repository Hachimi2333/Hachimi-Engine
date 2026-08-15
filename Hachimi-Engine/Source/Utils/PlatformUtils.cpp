#include "Utils/PlatformUtils.h"

#include <cstdlib>

#ifdef HE_PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

namespace HachimiEngine
{
    std::filesystem::path PlatformUtils::GetUserDocumentsDirectory()
    {
        if (const char* userProfile = std::getenv("USERPROFILE"))
        {
            return std::filesystem::path(userProfile) / "Documents";
        }
        return std::filesystem::current_path();
    }

    std::filesystem::path PlatformUtils::GetApplicationDataDirectory()
    {
        if (const char* appData = std::getenv("APPDATA"))
        {
            return std::filesystem::path(appData) / "HachimiEngine";
        }
        return std::filesystem::current_path() / "HachimiEngine";
    }

    std::filesystem::path PlatformUtils::GetDefaultProjectsDirectory()
    {
        return GetUserDocumentsDirectory() / "HachimiProjects";
    }

    void PlatformUtils::OpenPathInExplorer(const std::filesystem::path& path)
    {
#ifdef HE_PLATFORM_WINDOWS
        ShellExecuteA(nullptr, "open", path.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
}
