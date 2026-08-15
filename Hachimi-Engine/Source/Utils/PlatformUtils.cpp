#include "Utils/PlatformUtils.h"

#ifdef HE_PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

namespace HachimiEngine
{
    namespace
    {
        std::string GetEnvironmentVariableString(const char* name)
        {
#ifdef HE_PLATFORM_WINDOWS
            const DWORD length = GetEnvironmentVariableA(name, nullptr, 0);
            if (length == 0)
            {
                return {};
            }

            std::string value(length, '\0');
            GetEnvironmentVariableA(name, value.data(), length);
            value.resize(length - 1);
            return value;
#else
            if (const char* value = std::getenv(name))
            {
                return value;
            }
            return {};
#endif
        }
    }

    std::filesystem::path PlatformUtils::GetUserDocumentsDirectory()
    {
        const std::string userProfile = GetEnvironmentVariableString("USERPROFILE");
        if (!userProfile.empty())
        {
            return std::filesystem::path(userProfile) / "Documents";
        }
        return std::filesystem::current_path();
    }

    std::filesystem::path PlatformUtils::GetApplicationDataDirectory()
    {
        const std::string appData = GetEnvironmentVariableString("APPDATA");
        if (!appData.empty())
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
