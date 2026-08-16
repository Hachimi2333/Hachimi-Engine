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

    std::filesystem::path PlatformUtils::GetExecutableDirectory()
    {
#ifdef HE_PLATFORM_WINDOWS
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length > 0 && length == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length > 0)
        {
            return std::filesystem::path(buffer.substr(0, length)).parent_path();
        }
#endif
        return std::filesystem::current_path();
    }

    void PlatformUtils::OpenPathInExplorer(const std::filesystem::path& path)
    {
#ifdef HE_PLATFORM_WINDOWS
        ShellExecuteA(nullptr, "open", path.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
}
