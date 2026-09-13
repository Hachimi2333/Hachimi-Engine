#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // Thin wrapper around Native File Dialog Extended, which opens the Windows native file dialogs.
    // Every function blocks until the user confirms or cancels and returns an empty path on cancel.
    class FileDialogs
    {
    public:
        static std::filesystem::path OpenProjectFileDialog(const std::filesystem::path& startPath);
        static std::filesystem::path OpenDirectoryDialog(const std::filesystem::path& startPath);
        static std::filesystem::path OpenTextureImportDialog(const std::filesystem::path& startPath);
        // Every asset kind the project can hold, so one dialog covers importing a texture, a
        // material, a scene or a script.
        static std::filesystem::path OpenAssetImportDialog(const std::filesystem::path& startPath);
        static std::filesystem::path OpenSceneFileDialog(const std::filesystem::path& startPath);
        static std::filesystem::path OpenPackageFileDialog(const std::filesystem::path& startPath);
        static std::filesystem::path SaveFileDialog(const std::filesystem::path& startPath, const std::string& defaultName);
    };
}
