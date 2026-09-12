#include "Utils/VirtualFileSystem.h"

#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Packaging/PackageReader.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        struct Mount
        {
            MountInfo Info;
            Ref<PackageReader> Reader;
        };

        // Copy of the mount table taken under the lock so reads never hold it
        // while touching the disk. Holding the reader keeps it alive even when
        // another thread unmounts mid-read.
        struct MountSnapshot
        {
            MountInfo Info;
            Ref<PackageReader> Reader;
        };

        struct ResolvedLocation
        {
            bool Found = false;
            bool IsArchive = false;
            Ref<PackageReader> Reader;
            size_t EntryIndex = 0;
            std::filesystem::path DiskPath;
        };

        struct OutstandingRequest
        {
            bool Cancelled = false;
        };

        struct CompletedRequest
        {
            uint64_t Id = 0;
            bool Success = false;
            std::vector<uint8_t> Data;
            VirtualFileSystem::AsyncReadCallback Callback;
        };

        std::vector<Mount> s_Mounts;
        std::mutex s_MountMutex;

        std::atomic<uint64_t> s_NextRequestId{ 1 };
        std::mutex s_RequestMutex;
        std::unordered_map<uint64_t, OutstandingRequest> s_OutstandingRequests;
        std::deque<CompletedRequest> s_CompletedRequests;

        // Ceiling on requests that are queued but not yet dispatched.
        //
        // The completed queue is filled by workers and drained by PumpCompletedRequests, so a
        // caller that submits without pumping would grow it without bound. Refusing the request
        // up front is the honest failure: the caller learns its request was not queued, instead
        // of losing the callback later.
        constexpr size_t MaxOutstandingRequests = 4096;

        std::atomic<uint64_t> s_ReadCount{ 0 };
        std::atomic<uint64_t> s_BytesRead{ 0 };
        std::atomic<uint64_t> s_ArchiveReadCount{ 0 };
        std::atomic<uint64_t> s_DiskReadCount{ 0 };
        std::atomic<uint64_t> s_AsyncRequestCount{ 0 };

        std::string NormalizeForCompare(const std::filesystem::path& path)
        {
            std::error_code errorCode;
            std::filesystem::path normalized = std::filesystem::absolute(path, errorCode);
            if (errorCode)
            {
                normalized = path;
            }
            normalized = normalized.lexically_normal();

            std::string text = normalized.generic_string();
            // Drop a trailing separator so prefix matching stays component based,
            // but keep a bare drive root such as "D:/" intact.
            while (text.size() > 1 && text.back() == '/' && text[text.size() - 2] != ':')
            {
                text.pop_back();
            }

#ifdef HE_PLATFORM_WINDOWS
            text = ToLowerAscii(text);
#endif
            return text;
        }

        // Splits a path against a mount point. outRelative is empty when the path
        // is the mount point itself.
        bool SplitMountRelativeInclusive(const std::filesystem::path& path,
                                        const std::filesystem::path& mountPoint,
                                        std::string& outRelative)
        {
            const std::string pathText = NormalizeForCompare(path);
            const std::string mountText = NormalizeForCompare(mountPoint);
            if (mountText.empty() || pathText.size() < mountText.size())
            {
                return false;
            }

            if (pathText.compare(0, mountText.size(), mountText) != 0)
            {
                return false;
            }

            if (pathText.size() == mountText.size())
            {
                outRelative.clear();
                return true;
            }

            if (pathText[mountText.size()] != '/')
            {
                return false;
            }

            outRelative = pathText.substr(mountText.size() + 1);
            return !outRelative.empty();
        }

        std::vector<MountSnapshot> SnapshotMounts()
        {
            std::lock_guard<std::mutex> lock(s_MountMutex);
            std::vector<MountSnapshot> snapshot;
            snapshot.reserve(s_Mounts.size());
            for (const Mount& mount : s_Mounts)
            {
                snapshot.push_back({ mount.Info, mount.Reader });
            }
            return snapshot;
        }

        ResolvedLocation ResolveLocation(const std::filesystem::path& path)
        {
            ResolvedLocation location;

            for (const MountSnapshot& mount : SnapshotMounts())
            {
                std::string relative;
                if (!SplitMountRelativeInclusive(path, mount.Info.MountPoint, relative) || relative.empty())
                {
                    continue;
                }

                if (mount.Info.IsArchive && mount.Reader != nullptr)
                {
                    size_t entryIndex = 0;
                    if (mount.Reader->FindEntry(relative, entryIndex))
                    {
                        location.Found = true;
                        location.IsArchive = true;
                        location.Reader = mount.Reader;
                        location.EntryIndex = entryIndex;
                        return location;
                    }
                }
                else if (!mount.Info.SourceDirectory.empty())
                {
                    const std::filesystem::path diskPath =
                        mount.Info.SourceDirectory / std::filesystem::path(relative);
                    std::error_code errorCode;
                    if (std::filesystem::is_regular_file(diskPath, errorCode))
                    {
                        location.Found = true;
                        location.IsArchive = false;
                        location.DiskPath = diskPath;
                        return location;
                    }
                }
            }

            return location;
        }

        // Packaged directories exist only as entry name prefixes.
        bool AnyMountHasDirectory(const std::filesystem::path& path)
        {
            for (const MountSnapshot& mount : SnapshotMounts())
            {
                std::string relative;
                if (!SplitMountRelativeInclusive(path, mount.Info.MountPoint, relative))
                {
                    continue;
                }

                if (mount.Info.IsArchive && mount.Reader != nullptr)
                {
                    if (relative.empty() || mount.Reader->HasDirectory(relative))
                    {
                        return true;
                    }
                }
                else if (!mount.Info.SourceDirectory.empty())
                {
                    std::error_code errorCode;
                    if (std::filesystem::is_directory(mount.Info.SourceDirectory / std::filesystem::path(relative),
                            errorCode))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool ReadWholeDiskFile(const std::filesystem::path& path, std::vector<uint8_t>& outData)
        {
            outData.clear();

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
            {
                return false;
            }

            const std::streamoff size = file.tellg();
            if (size < 0)
            {
                return false;
            }

            outData.resize(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            if (size > 0)
            {
                file.read(reinterpret_cast<char*>(outData.data()), size);
                if (file.gcount() != size)
                {
                    outData.clear();
                    return false;
                }
            }
            return true;
        }

        bool ReadDiskFileRange(const std::filesystem::path& path, uint64_t offset, uint64_t size,
                               std::vector<uint8_t>& outData)
        {
            outData.clear();

            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }

            file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            if (!file)
            {
                return false;
            }

            outData.resize(static_cast<size_t>(size));
            if (size > 0)
            {
                file.read(reinterpret_cast<char*>(outData.data()), static_cast<std::streamsize>(size));
                if (file.gcount() != static_cast<std::streamsize>(size))
                {
                    outData.clear();
                    return false;
                }
            }
            return true;
        }

        std::string MakeRelativeKey(const std::filesystem::path& file, const std::filesystem::path& directory)
        {
            std::error_code errorCode;
            const std::filesystem::path relative = std::filesystem::relative(file, directory, errorCode);
            if (errorCode)
            {
                return {};
            }
            return ToLowerAscii(relative.generic_string());
        }

        std::vector<std::filesystem::path> EnumerateMerged(const std::filesystem::path& directory, bool recursive)
        {
            std::vector<std::filesystem::path> result;
            std::unordered_set<std::string> seen;

            for (const MountSnapshot& mount : SnapshotMounts())
            {
                std::string relativeDirectory;
                if (!SplitMountRelativeInclusive(directory, mount.Info.MountPoint, relativeDirectory))
                {
                    continue;
                }

                if (mount.Info.IsArchive && mount.Reader != nullptr)
                {
                    for (const std::string& name : mount.Reader->EnumerateEntries(relativeDirectory, recursive))
                    {
                        if (!seen.insert(ToLowerAscii(name)).second)
                        {
                            continue;
                        }
                        result.push_back(mount.Info.MountPoint / std::filesystem::path(name));
                    }
                }
                else if (!mount.Info.SourceDirectory.empty())
                {
                    const std::filesystem::path root = mount.Info.SourceDirectory;
                    const std::filesystem::path target =
                        relativeDirectory.empty() ? root : root / std::filesystem::path(relativeDirectory);

                    const std::vector<std::filesystem::path> files = recursive
                        ? FileSystem::GetFilesRecursive(target)
                        : FileSystem::GetFiles(target);
                    for (const std::filesystem::path& file : files)
                    {
                        const std::string key = MakeRelativeKey(file, root);
                        if (key.empty() || !seen.insert(key).second)
                        {
                            continue;
                        }
                        result.push_back(mount.Info.MountPoint / std::filesystem::path(key));
                    }
                }
            }

            // Loose files alongside the package fill any remaining gaps.
            const std::vector<std::filesystem::path> diskFiles = recursive
                ? FileSystem::GetFilesRecursive(directory)
                : FileSystem::GetFiles(directory);
            for (const std::filesystem::path& file : diskFiles)
            {
                const std::string key = MakeRelativeKey(file, directory);
                if (key.empty() || !seen.insert(key).second)
                {
                    continue;
                }
                result.push_back(file);
            }

            std::sort(result.begin(), result.end());
            return result;
        }

        void SortMountsByPriority()
        {
            std::stable_sort(s_Mounts.begin(), s_Mounts.end(),
                [](const Mount& lhs, const Mount& rhs) { return lhs.Info.Priority > rhs.Info.Priority; });
        }

        // Drops an existing mount of the same kind at the same mount point so a
        // second MountArchive() call replaces rather than stacks.
        void RemoveExistingMount(const std::filesystem::path& mountPoint, bool isArchive)
        {
            const std::string mountText = NormalizeForCompare(mountPoint);
            s_Mounts.erase(
                std::remove_if(s_Mounts.begin(), s_Mounts.end(),
                    [&mountText, isArchive](const Mount& mount)
                    {
                        return mount.Info.IsArchive == isArchive
                            && NormalizeForCompare(mount.Info.MountPoint) == mountText;
                    }),
                s_Mounts.end());
        }
    }

    bool VirtualFileSystem::MountArchive(const std::filesystem::path& archivePath,
                                         const std::filesystem::path& mountPoint,
                                         int priority)
    {
        Ref<PackageReader> reader = CreateRef<PackageReader>();
        if (!reader->Open(archivePath))
        {
            return false;
        }

        const size_t entryCount = reader->GetEntryCount();
        const std::filesystem::path normalizedMountPoint =
            std::filesystem::absolute(mountPoint).lexically_normal();

        Mount mount;
        mount.Info.ArchivePath = archivePath;
        mount.Info.MountPoint = normalizedMountPoint;
        mount.Info.Priority = priority;
        mount.Info.IsArchive = true;
        mount.Reader = std::move(reader);

        {
            std::lock_guard<std::mutex> lock(s_MountMutex);
            RemoveExistingMount(mount.Info.MountPoint, true);
            s_Mounts.push_back(std::move(mount));
            SortMountsByPriority();
        }

        HE_CORE_INFO("Mounted game package '{}' at {} ({} entries)",
            archivePath.string(), normalizedMountPoint.string(), entryCount);
        return true;
    }

    bool VirtualFileSystem::MountDirectory(const std::filesystem::path& sourceDirectory,
                                           const std::filesystem::path& mountPoint,
                                           int priority)
    {
        if (!FileSystem::IsDirectory(sourceDirectory))
        {
            HE_CORE_ERROR("Cannot mount missing directory: {}", sourceDirectory.string());
            return false;
        }

        const std::filesystem::path normalizedSource =
            std::filesystem::absolute(sourceDirectory).lexically_normal();
        const std::filesystem::path normalizedMountPoint =
            std::filesystem::absolute(mountPoint).lexically_normal();

        Mount mount;
        mount.Info.SourceDirectory = normalizedSource;
        mount.Info.MountPoint = normalizedMountPoint;
        mount.Info.Priority = priority;
        mount.Info.IsArchive = false;

        {
            std::lock_guard<std::mutex> lock(s_MountMutex);
            RemoveExistingMount(mount.Info.MountPoint, false);
            s_Mounts.push_back(std::move(mount));
            SortMountsByPriority();
        }

        HE_CORE_INFO("Mounted loose content '{}' at {}",
            normalizedSource.string(), normalizedMountPoint.string());
        return true;
    }

    void VirtualFileSystem::UnmountAll()
    {
        // Dropping outstanding requests first guarantees no callback fires after
        // teardown; readers stay alive until in-flight reads release their Ref.
        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            s_OutstandingRequests.clear();
            s_CompletedRequests.clear();
        }
        {
            std::lock_guard<std::mutex> lock(s_MountMutex);
            s_Mounts.clear();
        }
    }

    bool VirtualFileSystem::IsArchiveMounted()
    {
        std::lock_guard<std::mutex> lock(s_MountMutex);
        for (const Mount& mount : s_Mounts)
        {
            if (mount.Info.IsArchive)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<MountInfo> VirtualFileSystem::GetMounts()
    {
        std::lock_guard<std::mutex> lock(s_MountMutex);
        std::vector<MountInfo> mounts;
        mounts.reserve(s_Mounts.size());
        for (const Mount& mount : s_Mounts)
        {
            mounts.push_back(mount.Info);
        }
        return mounts;
    }

    std::filesystem::path VirtualFileSystem::GetDataRoot()
    {
        {
            std::lock_guard<std::mutex> lock(s_MountMutex);
            if (!s_Mounts.empty())
            {
                return s_Mounts.front().Info.MountPoint;
            }
        }
        return PlatformUtils::GetExecutableDirectory();
    }

    bool VirtualFileSystem::Exists(const std::filesystem::path& path)
    {
        if (ResolveLocation(path).Found)
        {
            return true;
        }
        if (AnyMountHasDirectory(path))
        {
            return true;
        }
        return FileSystem::Exists(path);
    }

    bool VirtualFileSystem::IsDirectory(const std::filesystem::path& path)
    {
        if (ResolveLocation(path).Found)
        {
            return false;
        }
        if (AnyMountHasDirectory(path))
        {
            return true;
        }
        return FileSystem::IsDirectory(path);
    }

    uint64_t VirtualFileSystem::GetFileSize(const std::filesystem::path& path)
    {
        const ResolvedLocation location = ResolveLocation(path);
        if (location.Found)
        {
            if (location.IsArchive)
            {
                return location.Reader->GetEntry(location.EntryIndex).UncompressedSize;
            }
            return FileSystem::GetFileSize(location.DiskPath);
        }
        return FileSystem::GetFileSize(path);
    }

    bool VirtualFileSystem::IsPackagedPath(const std::filesystem::path& path)
    {
        const ResolvedLocation location = ResolveLocation(path);
        return location.Found && location.IsArchive;
    }

    bool VirtualFileSystem::ReadTextFile(const std::filesystem::path& path, std::string& outText)
    {
        std::vector<uint8_t> data;
        if (!ReadBinaryFile(path, data))
        {
            outText.clear();
            return false;
        }

        outText.assign(data.empty() ? "" : reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    bool VirtualFileSystem::ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& outData)
    {
        outData.clear();

        const ResolvedLocation location = ResolveLocation(path);
        bool success = false;

        if (location.Found)
        {
            if (location.IsArchive)
            {
                success = location.Reader->ReadEntry(location.EntryIndex, outData);
                s_ArchiveReadCount.fetch_add(1);
            }
            else
            {
                success = ReadWholeDiskFile(location.DiskPath, outData);
                s_DiskReadCount.fetch_add(1);
            }
        }
        else
        {
            success = ReadWholeDiskFile(path, outData);
            s_DiskReadCount.fetch_add(1);
        }

        if (success)
        {
            s_ReadCount.fetch_add(1);
            s_BytesRead.fetch_add(outData.size());
        }
        return success;
    }

    bool VirtualFileSystem::ReadFileRange(const std::filesystem::path& path, uint64_t offset, uint64_t size,
                                          std::vector<uint8_t>& outData)
    {
        outData.clear();

        const ResolvedLocation location = ResolveLocation(path);
        if (location.Found && location.IsArchive)
        {
            const bool success = location.Reader->ReadEntryRange(location.EntryIndex, offset, size, outData);
            if (success)
            {
                s_ReadCount.fetch_add(1);
                s_BytesRead.fetch_add(outData.size());
                s_ArchiveReadCount.fetch_add(1);
            }
            return success;
        }

        const std::filesystem::path diskPath = location.Found ? location.DiskPath : path;
        const bool success = ReadDiskFileRange(diskPath, offset, size, outData);
        if (success)
        {
            s_ReadCount.fetch_add(1);
            s_BytesRead.fetch_add(outData.size());
            s_DiskReadCount.fetch_add(1);
        }
        return success;
    }

    bool VirtualFileSystem::MapFile(const std::filesystem::path& path, FileMapping& outMapping)
    {
        outMapping.Reset();

        const ResolvedLocation location = ResolveLocation(path);
        if (location.Found && location.IsArchive)
        {
            // The reader is handed to the mapping: a borrowed view into the package's memory
            // map must keep that map alive even after the mount is released.
            const bool success = location.Reader->MapEntry(location.EntryIndex, outMapping, location.Reader);
            if (success)
            {
                s_ReadCount.fetch_add(1);
                s_BytesRead.fetch_add(outMapping.Size());
                s_ArchiveReadCount.fetch_add(1);
            }
            return success;
        }

        std::vector<uint8_t> data;
        const bool success = ReadWholeDiskFile(location.Found ? location.DiskPath : path, data);
        if (!success)
        {
            return false;
        }

        s_ReadCount.fetch_add(1);
        s_BytesRead.fetch_add(data.size());
        s_DiskReadCount.fetch_add(1);
        outMapping = FileMapping::Own(std::move(data));
        return true;
    }

    std::vector<std::filesystem::path> VirtualFileSystem::GetFiles(const std::filesystem::path& directory)
    {
        return EnumerateMerged(directory, false);
    }

    std::vector<std::filesystem::path> VirtualFileSystem::GetFilesRecursive(const std::filesystem::path& directory)
    {
        return EnumerateMerged(directory, true);
    }

    uint64_t VirtualFileSystem::ReadFileAsync(const std::filesystem::path& path, AsyncReadCallback callback)
    {
        if (!callback)
        {
            return 0;
        }

        const uint64_t requestId = s_NextRequestId.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            if (s_OutstandingRequests.size() >= MaxOutstandingRequests)
            {
                HE_CORE_ERROR("Refusing async read of '{}': {} requests are already pending; is "
                              "PumpCompletedRequests being called?",
                    path.string(),
                    MaxOutstandingRequests);
                return 0;
            }
            s_OutstandingRequests.emplace(requestId, OutstandingRequest{});
        }
        s_AsyncRequestCount.fetch_add(1);

        JobSystem::Submit([requestId, path, callback = std::move(callback)]() mutable
        {
            std::vector<uint8_t> data;
            const bool success = ReadBinaryFile(path, data);

            std::lock_guard<std::mutex> lock(s_RequestMutex);
            if (s_OutstandingRequests.find(requestId) == s_OutstandingRequests.end())
            {
                // Dropped before the read finished; no callback may fire.
                return;
            }
            s_CompletedRequests.push_back(CompletedRequest{ requestId, success, std::move(data), std::move(callback) });
        });

        return requestId;
    }

    void VirtualFileSystem::CancelRequest(uint64_t requestId)
    {
        std::lock_guard<std::mutex> lock(s_RequestMutex);
        const auto found = s_OutstandingRequests.find(requestId);
        if (found != s_OutstandingRequests.end())
        {
            found->second.Cancelled = true;
        }
    }

    bool VirtualFileSystem::IsRequestPending(uint64_t requestId)
    {
        std::lock_guard<std::mutex> lock(s_RequestMutex);
        return s_OutstandingRequests.find(requestId) != s_OutstandingRequests.end();
    }

    size_t VirtualFileSystem::GetPendingRequestCount()
    {
        std::lock_guard<std::mutex> lock(s_RequestMutex);
        return s_OutstandingRequests.size();
    }

    void VirtualFileSystem::PumpCompletedRequests()
    {
        std::deque<CompletedRequest> ready;
        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            ready.swap(s_CompletedRequests);
        }

        for (CompletedRequest& request : ready)
        {
            bool cancelled = true;
            {
                std::lock_guard<std::mutex> lock(s_RequestMutex);
                const auto found = s_OutstandingRequests.find(request.Id);
                if (found != s_OutstandingRequests.end())
                {
                    cancelled = found->second.Cancelled;
                    s_OutstandingRequests.erase(found);
                }
            }

            if (cancelled || !request.Callback)
            {
                continue;
            }

            request.Callback(request.Success, std::move(request.Data));
        }
    }

    VirtualFileSystem::Statistics VirtualFileSystem::GetStatistics()
    {
        Statistics statistics;
        statistics.ReadCount = s_ReadCount.load();
        statistics.BytesRead = s_BytesRead.load();
        statistics.ArchiveReadCount = s_ArchiveReadCount.load();
        statistics.DiskReadCount = s_DiskReadCount.load();
        statistics.AsyncRequestCount = s_AsyncRequestCount.load();
        return statistics;
    }

    void VirtualFileSystem::ResetStatistics()
    {
        s_ReadCount.store(0);
        s_BytesRead.store(0);
        s_ArchiveReadCount.store(0);
        s_DiskReadCount.store(0);
        s_AsyncRequestCount.store(0);
    }
}
