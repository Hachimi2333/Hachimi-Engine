#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    // Thin std::filesystem wrapper so path operations stay in one place.
    class FileSystem
    {
    public:
        static bool Exists(const std::filesystem::path& path);
        static bool IsDirectory(const std::filesystem::path& path);
        static bool CreateDirectories(const std::filesystem::path& path);

        static std::string GetFileName(const std::filesystem::path& path);
        static std::string GetFileNameWithoutExtension(const std::filesystem::path& path);
        static std::string GetExtension(const std::filesystem::path& path);
        static std::filesystem::path GetParentPath(const std::filesystem::path& path);

        static std::vector<std::filesystem::path> GetFiles(const std::filesystem::path& directory);
        static std::vector<std::filesystem::path> GetDirectories(const std::filesystem::path& directory);

        static bool CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination);
    };
}
