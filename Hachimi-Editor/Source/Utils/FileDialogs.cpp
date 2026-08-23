#include "Utils/FileDialogs.h"

#include "Core/Log.h"

#include <nfd.h>

#include <array>
#include <cstdlib>
#include <span>
#include <string>

namespace HachimiEngine
{
    namespace
    {
        // Converts a UTF-8 path returned by NFD into the native filesystem path type.
        std::filesystem::path ToNativePath(const char* utf8Path)
        {
            if (utf8Path == nullptr || utf8Path[0] == '\0')
            {
                return {};
            }

            return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8Path)));
        }

        // Converts a native filesystem path into the UTF-8 string expected by the NFD U8 API.
        std::string ToUtf8(const std::filesystem::path& path)
        {
            const std::u8string utf8Path = path.u8string();
            return { reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size() };
        }

        nfdresult_t InitializeNfd()
        {
            const nfdresult_t result = NFD_Init();
            if (result != NFD_OKAY)
            {
                HE_CLIENT_ERROR("Failed to initialize native file dialogs: {}", NFD_GetError());
                return result;
            }

            std::atexit([] { NFD_Quit(); });
            return result;
        }

        bool IsNfdReady()
        {
            static const nfdresult_t initResult = InitializeNfd();
            return initResult == NFD_OKAY;
        }

        // Shared single-file open dialog. Pass an empty filters array to show every file type.
        std::filesystem::path OpenFileDialog(
            const std::filesystem::path& startPath,
            std::span<const nfdu8filteritem_t> filters)
        {
            if (!IsNfdReady())
            {
                return {};
            }

            const std::string defaultPath = ToUtf8(startPath);
            nfdu8char_t* selectedPath = nullptr;
            const nfdresult_t result = NFD_OpenDialogU8(
                &selectedPath,
                filters.data(),
                static_cast<nfdfiltersize_t>(filters.size()),
                defaultPath.empty() ? nullptr : defaultPath.c_str());

            if (result == NFD_CANCEL)
            {
                return {};
            }

            if (result == NFD_ERROR)
            {
                HE_CLIENT_ERROR("Native file dialog failed: {}", NFD_GetError());
                return {};
            }

            const std::filesystem::path path = ToNativePath(selectedPath);
            NFD_FreePathU8(selectedPath);
            return path;
        }
    }

    std::filesystem::path FileDialogs::OpenProjectFileDialog(const std::filesystem::path& startPath)
    {
        constexpr std::array filters = {
            nfdu8filteritem_t{ "Hachimi Project", "hproj" }
        };
        return OpenFileDialog(startPath, filters);
    }

    std::filesystem::path FileDialogs::OpenDirectoryDialog(const std::filesystem::path& startPath)
    {
        if (!IsNfdReady())
        {
            return {};
        }

        const std::string defaultPath = ToUtf8(startPath);
        nfdu8char_t* selectedPath = nullptr;
        const nfdresult_t result = NFD_PickFolderU8(
            &selectedPath,
            defaultPath.empty() ? nullptr : defaultPath.c_str());

        if (result == NFD_CANCEL)
        {
            return {};
        }

        if (result == NFD_ERROR)
        {
            HE_CLIENT_ERROR("Native folder dialog failed: {}", NFD_GetError());
            return {};
        }

        const std::filesystem::path path = ToNativePath(selectedPath);
        NFD_FreePathU8(selectedPath);
        return path;
    }

    std::filesystem::path FileDialogs::OpenTextureImportDialog(const std::filesystem::path& startPath)
    {
        constexpr std::array filters = {
            nfdu8filteritem_t{ "Image files", "png,jpg,jpeg,tga,bmp" }
        };
        return OpenFileDialog(startPath, filters);
    }

    std::filesystem::path FileDialogs::OpenSceneFileDialog(const std::filesystem::path& startPath)
    {
        constexpr std::array filters = {
            nfdu8filteritem_t{ "Hachimi Scene", "hscene" }
        };
        return OpenFileDialog(startPath, filters);
    }
}
