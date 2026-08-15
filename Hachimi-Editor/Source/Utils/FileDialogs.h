#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // Small ImGuiFileDialog wrapper used by the editor panels.
    class FileDialogs
    {
    public:
        static void OpenProjectFileDialog(const std::filesystem::path& startPath);
        static bool DrawProjectFileDialog(std::string& selectedPath);

        static void OpenDirectoryDialog(const std::filesystem::path& startPath);
        static bool DrawDirectoryDialog(std::string& selectedPath);

        static void OpenTextureImportDialog(const std::filesystem::path& startPath);
        static bool DrawTextureImportDialog(std::string& selectedPath);

        static void OpenSceneFileDialog(const std::filesystem::path& startPath);
        static bool DrawSceneFileDialog(std::string& selectedPath);
    };
}
