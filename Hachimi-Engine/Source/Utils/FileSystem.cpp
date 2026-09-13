#include "Utils/FileSystem.h"

#include <algorithm>
#include <fstream>

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

    bool FileSystem::IsFile(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        return std::filesystem::is_regular_file(path, errorCode);
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

    std::vector<std::filesystem::path> FileSystem::GetFilesRecursive(const std::filesystem::path& directory)
    {
        std::vector<std::filesystem::path> files;
        std::error_code errorCode;
        std::filesystem::recursive_directory_iterator iterator(directory, std::filesystem::directory_options::skip_permission_denied, errorCode);
        const std::filesystem::recursive_directory_iterator end;
        for (; iterator != end; iterator.increment(errorCode))
        {
            if (errorCode)
            {
                errorCode.clear();
                continue;
            }

            if (iterator->is_regular_file(errorCode))
            {
                files.push_back(iterator->path());
                errorCode.clear();
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

    bool FileSystem::MoveFile(const std::filesystem::path& source, const std::filesystem::path& destination)
    {
        std::error_code errorCode;
        std::filesystem::rename(source, destination, errorCode);
        if (!errorCode)
        {
            return true;
        }

        // rename() cannot cross volumes; a copy plus a delete is the portable equivalent. Both
        // forms move files and directories alike, which is what the asset operations need.
        errorCode.clear();
        std::filesystem::copy(source, destination,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            errorCode);
        if (errorCode)
        {
            return false;
        }

        errorCode.clear();
        std::filesystem::remove_all(source, errorCode);
        return !errorCode;
    }

    bool FileSystem::WriteTextFile(const std::filesystem::path& path, std::string_view content)
    {
        CreateDirectories(path.parent_path());

        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return false;
        }

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file.good();
    }

    bool FileSystem::WriteBinaryFile(const std::filesystem::path& path, const void* data, size_t size)
    {
        CreateDirectories(path.parent_path());

        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return false;
        }

        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.good();
    }

    bool FileSystem::RemoveAll(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::remove_all(path, errorCode);
        return !errorCode;
    }

    uintmax_t FileSystem::GetFileSize(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        const uintmax_t size = std::filesystem::file_size(path, errorCode);
        return errorCode ? 0 : size;
    }

    std::filesystem::file_time_type FileSystem::GetLastWriteTime(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, errorCode);
        return errorCode ? std::filesystem::file_time_type{} : time;
    }
}
