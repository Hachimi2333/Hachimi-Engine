#pragma once

#include "Core/Base.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
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
        static std::vector<std::filesystem::path> GetFilesRecursive(const std::filesystem::path& directory);
        static std::vector<std::filesystem::path> GetDirectories(const std::filesystem::path& directory);

        static bool CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination);
        // Renames a file or a directory, falling back to copy-then-delete when the source and
        // the destination are on different volumes.
        static bool MoveFile(const std::filesystem::path& source, const std::filesystem::path& destination);
        static bool WriteTextFile(const std::filesystem::path& path, std::string_view content);
        static bool WriteBinaryFile(const std::filesystem::path& path, const void* data, size_t size);
        static bool RemoveAll(const std::filesystem::path& path);

        // True when the path is a regular file.
        static bool IsFile(const std::filesystem::path& path);
        static uintmax_t GetFileSize(const std::filesystem::path& path);
        static std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& path);
    };
}
