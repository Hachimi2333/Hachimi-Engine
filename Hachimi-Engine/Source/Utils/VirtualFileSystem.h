#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Utils/FileMapping.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace HachimiEngine
{
    struct MountInfo
    {
        // Empty for directory mounts.
        std::filesystem::path ArchivePath;
        std::filesystem::path SourceDirectory;
        std::filesystem::path MountPoint;
        int Priority = 0;
        bool IsArchive = false;
    };

    // Read-only virtual file system.
    //
    // A path is resolved against mounted game packages first (highest priority
    // wins, which is how patch and loose-content overlays work) and falls back to
    // the operating system file system when nothing matches. With no mount at all
    // every call is equivalent to a direct disk read, so editor code needs no
    // special casing.
    class VirtualFileSystem
    {
    public:
        // Runs on a worker thread; success is false when the entry is missing or
        // cannot be read.
        using AsyncReadCallback = std::function<void(bool success, std::vector<uint8_t>&& data)>;

        struct Statistics
        {
            uint64_t ReadCount = 0;
            uint64_t BytesRead = 0;
            uint64_t ArchiveReadCount = 0;
            uint64_t DiskReadCount = 0;
            uint64_t AsyncRequestCount = 0;
        };

        // Serves paths at or below mountPoint from the archive. Replaces any
        // previously mounted archive.
        static bool MountArchive(const std::filesystem::path& archivePath,
                                 const std::filesystem::path& mountPoint,
                                 int priority = 0);
        // Serves paths at or below mountPoint from a directory of loose files.
        static bool MountDirectory(const std::filesystem::path& sourceDirectory,
                                   const std::filesystem::path& mountPoint,
                                   int priority = 0);
        // Drops every mount and discards pending asynchronous requests without
        // invoking their callbacks.
        static void UnmountAll();
        static bool IsArchiveMounted();
        // Mounts ordered by descending priority.
        static std::vector<MountInfo> GetMounts();

        // Directory that provides "Shaders/" and "Assets/Fonts/" at runtime: the
        // active mount point when something is mounted, else the executable
        // directory. Replaces PlatformUtils::GetRuntimeDataDirectory().
        static std::filesystem::path GetDataRoot();

        static bool Exists(const std::filesystem::path& path);
        static bool IsDirectory(const std::filesystem::path& path);
        static uint64_t GetFileSize(const std::filesystem::path& path);
        // True when the path resolves to a package entry, which is read-only.
        static bool IsPackagedPath(const std::filesystem::path& path);

        static bool ReadTextFile(const std::filesystem::path& path, std::string& outText);
        static bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& outData);
        static bool ReadFileRange(const std::filesystem::path& path, uint64_t offset, uint64_t size,
                                  std::vector<uint8_t>& outData);
        // Zero copy for stored entries of a memory mapped package.
        static bool MapFile(const std::filesystem::path& path, FileMapping& outMapping);

        // Merged across mounts and the disk, deduplicated by relative name with
        // higher priority winning, sorted. Empty when nothing matches.
        static std::vector<std::filesystem::path> GetFiles(const std::filesystem::path& directory);
        static std::vector<std::filesystem::path> GetFilesRecursive(const std::filesystem::path& directory);

        // Reads and decompresses on a worker thread. The callback runs on the
        // main thread from PumpCompletedRequests(). Returns 0 when the request
        // could not be queued.
        static uint64_t ReadFileAsync(const std::filesystem::path& path, AsyncReadCallback callback);
        static void CancelRequest(uint64_t requestId);
        static bool IsRequestPending(uint64_t requestId);
        static size_t GetPendingRequestCount();
        // Dispatches callbacks for finished requests. Call once per frame.
        static void PumpCompletedRequests();

        static Statistics GetStatistics();
        static void ResetStatistics();
    };
}
