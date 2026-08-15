#include "Utils/FileSystem.h"

#include <algorithm>

namespace HachimiEngine
{
    bool FileSystem::Exists(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        return std::filesystem::exists(path, errorCode);
    }

    bool FileSystem::IsDirectory(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        return std::filesystem::is_directory(path, errorCode);
    }

    bool FileSystem::CreateDirectories(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::create_directories(path, errorCode);
        return !errorCode;
    }

    std::string FileSystem::GetFileName(const std::filesystem::path& path)
    {
        return path.filename().string();
    }

    std::string FileSystem::GetFileNameWithoutExtension(const std::filesystem::path& path)
    {
        return path.stem().string();
    }

    std::string FileSystem::GetExtension(const std::filesystem::path& path)
    {
        return path.extension().string();
    }

    std::filesystem::path FileSystem::GetParentPath(const std::filesystem::path& path)
    {
        return path.parent_path();
    }

    std::vector<std::filesystem::path> FileSystem::GetFiles(const std::filesystem::path& directory)
    {
        std::vector<std::filesystem::path> files;
        std::error_code errorCode;
        for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode))
        {
            if (entry.is_regular_file(errorCode))
            {
                files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    std::vector<std::filesystem::path> FileSystem::GetDirectories(const std::filesystem::path& directory)
    {
        std::vector<std::filesystem::path> directories;
        std::error_code errorCode;
        for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode))
        {
            if (entry.is_directory(errorCode))
            {
                directories.push_back(entry.path());
            }
        }

        std::sort(directories.begin(), directories.end());
        return directories;
    }

    bool FileSystem::CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination)
    {
        std::error_code errorCode;
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, errorCode);
        return !errorCode;
    }
}
