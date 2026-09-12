// VirtualFileSystem: mounting, lookups, listings, overlays and the disk fallback.

#include <doctest/doctest.h>

#include "Support/TestWorkspace.h"
#include "Utils/FileMapping.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE_BEGIN("Utils");

TEST_CASE("a mounted package resolves every entry")
{
    const TestWorkspace& workspace = Workspace();

    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());
    CHECK(VirtualFileSystem::IsArchiveMounted());
    CHECK(VirtualFileSystem::GetMounts().size() == 1);
    CHECK(VirtualFileSystem::GetDataRoot() == std::filesystem::absolute(mount.MountPoint()).lexically_normal());

    // Packaged directories exist only as entry name prefixes.
    CHECK(VirtualFileSystem::Exists(mount.MountPoint() / "Data"));
    CHECK(VirtualFileSystem::IsDirectory(mount.MountPoint() / "Data"));
    CHECK_FALSE(VirtualFileSystem::Exists(mount.MountPoint() / "Data/nope.bin"));
    CHECK_FALSE(VirtualFileSystem::IsPackagedPath(mount.MountPoint() / "Data/nope.bin"));

    for (const Sample& sample : workspace.Samples())
    {
        INFO("entry: ", sample.VirtualPath);

        const std::filesystem::path virtualPath = mount.MountPoint() / std::filesystem::path(sample.VirtualPath);
        CHECK(VirtualFileSystem::Exists(virtualPath));
        CHECK(VirtualFileSystem::IsPackagedPath(virtualPath));
        CHECK(VirtualFileSystem::GetFileSize(virtualPath) == sample.Content.size());

        std::vector<uint8_t> bytes;
        if (!VirtualFileSystem::ReadBinaryFile(virtualPath, bytes))
        {
            FAIL_CHECK("VFS read failed");
        }
        else
        {
            CHECK(SameBytes(bytes, sample.Content));
        }

        FileMapping mapping;
        if (!VirtualFileSystem::MapFile(virtualPath, mapping))
        {
            FAIL_CHECK("VFS map failed");
        }
        else
        {
            CHECK(mapping.Size() == sample.Content.size());
        }

        if (sample.Content.size() > 8)
        {
            std::vector<uint8_t> range;
            CHECK(VirtualFileSystem::ReadFileRange(virtualPath, 3, 4, range));
            CHECK(range.size() == 4);
            CHECK(SameSpan(range, sample.Content, 3));
        }
    }
}

TEST_CASE("lookup normalization follows Windows path semantics")
{
    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());

    // Archive lookups are case-insensitive and tolerate a "./" prefix.
    CHECK(VirtualFileSystem::Exists(mount.MountPoint() / "data/MULTI_BLOCK.BIN"));
    CHECK(VirtualFileSystem::Exists(mount.MountPoint() / "./Data/one_byte.bin"));

    // A path that escapes the mount point must not resolve to anything.
    CHECK_FALSE(VirtualFileSystem::Exists(mount.MountPoint() / "Data/../../escape.bin"));
}

TEST_CASE("directory enumeration merges the archive listing")
{
    const TestWorkspace& workspace = Workspace();

    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());
    const std::filesystem::path mountPoint = mount.MountPoint();

    size_t expectedRootFiles = 0;
    for (const Sample& sample : workspace.Samples())
    {
        if (std::filesystem::path(sample.VirtualPath).parent_path() == std::filesystem::path("Data"))
        {
            ++expectedRootFiles;
        }
    }

    const std::vector<std::filesystem::path> rootFiles = VirtualFileSystem::GetFiles(mountPoint / "Data");
    CHECK(rootFiles.size() == expectedRootFiles);
    CHECK(std::is_sorted(rootFiles.begin(), rootFiles.end()));
    for (const std::filesystem::path& file : rootFiles)
    {
        INFO("listed: ", file.string());
        CHECK(file.parent_path() == mountPoint / "Data");
    }

    // Recursive listing inside the package namespaces, where no disk content can shadow
    // the archive.
    const std::vector<std::filesystem::path> recursiveData =
        VirtualFileSystem::GetFilesRecursive(mountPoint / "Data");
    CHECK(recursiveData.size() == expectedRootFiles);

    const std::vector<std::filesystem::path> recursiveAssets =
        VirtualFileSystem::GetFilesRecursive(mountPoint / "Assets");
    CHECK(recursiveAssets.size() == workspace.Samples().size() - expectedRootFiles);

    // At the mount point the archive is merged with any loose files sitting next to the
    // package, so the listing is a superset of the archive.
    const std::vector<std::filesystem::path> recursiveRoot =
        VirtualFileSystem::GetFilesRecursive(mountPoint);
    CHECK(recursiveRoot.size() >= workspace.Samples().size() + 1);

    CHECK(VirtualFileSystem::GetFiles(mountPoint / ScriptsEntryDirectory).size() == ScriptSampleCount);
}

TEST_CASE("a higher priority directory mount shadows the package")
{
    const TestWorkspace& workspace = Workspace();
    const Sample* sample = workspace.FindSample("Data/one_byte.bin");
    REQUIRE(sample != nullptr);

    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());
    const std::filesystem::path mountPoint = mount.MountPoint();

    const std::filesystem::path overlayDirectory = workspace.PrepareDirectory("overlay_content");
    const std::vector<uint8_t> overlayBytes { 'o', 'v', 'e', 'r', 'l', 'a', 'y' };
    REQUIRE(WriteBytes(overlayDirectory / "Data" / "one_byte.bin", overlayBytes));

    REQUIRE(VirtualFileSystem::MountDirectory(overlayDirectory, mountPoint, 10));
    CHECK(VirtualFileSystem::GetMounts().size() == 2);
    REQUIRE_FALSE(VirtualFileSystem::GetMounts().empty());
    CHECK(VirtualFileSystem::GetMounts().front().Priority == 10);

    std::vector<uint8_t> shadowed;
    REQUIRE(VirtualFileSystem::ReadBinaryFile(mountPoint / "Data/one_byte.bin", shadowed));
    CHECK(SameBytes(shadowed, overlayBytes));

    std::vector<uint8_t> untouched;
    REQUIRE(VirtualFileSystem::ReadBinaryFile(mountPoint / "Data/empty.bin", untouched));
    CHECK(untouched.empty());
    CHECK(VirtualFileSystem::IsDirectory(mountPoint / "Data"));

    VirtualFileSystem::UnmountAll();
    CHECK_FALSE(VirtualFileSystem::IsArchiveMounted());
    CHECK(VirtualFileSystem::GetMounts().empty());

    // Re-mounting the package shows the original entry again: the overlay only ever
    // shadowed it.
    REQUIRE(VirtualFileSystem::MountArchive(workspace.PackagePath(), mountPoint));
    std::vector<uint8_t> restored;
    REQUIRE(VirtualFileSystem::ReadBinaryFile(mountPoint / "Data/one_byte.bin", restored));
    CHECK(SameBytes(restored, sample->Content));
}

TEST_CASE("with nothing mounted the virtual file system is the disk")
{
    const TestWorkspace& workspace = Workspace();

    VirtualFileSystem::UnmountAll();
    REQUIRE_FALSE(VirtualFileSystem::IsArchiveMounted());
    CHECK(VirtualFileSystem::GetMounts().empty());

    const Sample& first = workspace.Samples().front();

    std::vector<uint8_t> viaVfs;
    REQUIRE(VirtualFileSystem::ReadBinaryFile(first.SourcePath, viaVfs));
    CHECK(SameBytes(viaVfs, first.Content));

    CHECK(VirtualFileSystem::GetFileSize(first.SourcePath) == FileSystem::GetFileSize(first.SourcePath));
    CHECK(VirtualFileSystem::GetFiles(first.SourcePath.parent_path()).size()
        == FileSystem::GetFiles(first.SourcePath.parent_path()).size());
    CHECK_FALSE(VirtualFileSystem::IsPackagedPath(first.SourcePath));
}

TEST_CASE("reading a package never extracts it")
{
    const std::filesystem::path extractionCache = PlatformUtils::GetLocalAppDataDirectory() / "Packages";
    const bool cacheExistedBefore = FileSystem::Exists(extractionCache);

    VirtualFileSystem::ResetStatistics();
    VirtualFileSystem::UnmountAll();
    {
        ScopedArchiveMount mount;
        REQUIRE(mount.IsMounted());

        std::vector<uint8_t> bytes;
        REQUIRE(VirtualFileSystem::ReadBinaryFile(mount.MountPoint() / MultiBlockEntry, bytes));
        CHECK_FALSE(bytes.empty());
    }

    CHECK_FALSE(VirtualFileSystem::IsArchiveMounted());

    const VirtualFileSystem::Statistics statistics = VirtualFileSystem::GetStatistics();
    MESSAGE("reads: ", statistics.ReadCount, ", bytes: ", statistics.BytesRead, ", archive: ",
        statistics.ArchiveReadCount, ", disk: ", statistics.DiskReadCount, ", async: ",
        statistics.AsyncRequestCount);

    CHECK(statistics.ArchiveReadCount > 0);
    if (cacheExistedBefore)
    {
        MESSAGE("the extraction cache already existed before the run; creation check skipped");
    }
    else
    {
        CHECK(FileSystem::Exists(extractionCache) == cacheExistedBefore);
    }
}

TEST_SUITE_END();
