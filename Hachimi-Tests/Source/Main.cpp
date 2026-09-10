// Headless verification for the game package container and the virtual file
// system. The engine forbids automated GUI testing, so every assertion here runs
// without a window and the process exit code is the result.

#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Packaging/PackageFormat.h"
#include "Packaging/PackageEntryStream.h"
#include "Packaging/PackageReader.h"
#include "Packaging/PackageWriter.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"
#include "Utils/VirtualFileSystem.h"
#include "Utils/XXHash.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

// A single translation unit exercising engine types directly; qualifying every
// reference would only add noise here.
using namespace HachimiEngine;

namespace
{
    constexpr uint32_t TestBlockSize = 4096;
    constexpr const char* MultiBlockPath = "Data/multi_block.bin";
    constexpr const char* RandomPath = "Data/random.bin";

    int g_CheckCount = 0;
    int g_FailureCount = 0;

    void Section(const char* name)
    {
        std::printf("\n== %s ==\n", name);
    }

    void Check(bool condition, const std::string& message)
    {
        ++g_CheckCount;
        if (!condition)
        {
            ++g_FailureCount;
            std::printf("  [FAIL] %s\n", message.c_str());
        }
    }

    size_t ExpectedBlockCount(size_t size)
    {
        return size == 0 ? 0 : (size + TestBlockSize - 1) / TestBlockSize;
    }

    std::vector<uint8_t> MakeRandomBytes(size_t size, uint32_t seed)
    {
        std::mt19937 generator(seed);
        std::uniform_int_distribution<int> distribution(0, 255);

        std::vector<uint8_t> bytes(size);
        for (uint8_t& byte : bytes)
        {
            byte = static_cast<uint8_t>(distribution(generator));
        }
        return bytes;
    }

    std::vector<uint8_t> MakeCompressibleBytes(size_t size)
    {
        const std::string pattern = "Hachimi-Engine asset payload; zstd compresses this very well.\n";
        std::vector<uint8_t> bytes;
        bytes.reserve(size);
        while (bytes.size() < size)
        {
            const size_t chunk = std::min(pattern.size(), size - bytes.size());
            bytes.insert(bytes.end(), pattern.begin(), pattern.begin() + static_cast<ptrdiff_t>(chunk));
        }
        return bytes;
    }

    bool WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
    {
        return FileSystem::WriteBinaryFile(path, data.data(), data.size());
    }

    bool SameBytes(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs)
    {
        return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    bool SameSpan(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs, size_t rhsOffset)
    {
        return rhsOffset + lhs.size() <= rhs.size()
            && std::equal(lhs.begin(), lhs.end(), rhs.begin() + static_cast<ptrdiff_t>(rhsOffset));
    }

    // One synthetic asset: its virtual path inside the package and its content.
    struct Sample
    {
        std::string VirtualPath;
        std::filesystem::path SourcePath;
        std::vector<uint8_t> Content;
        bool ExpectStore = false;
    };

    const Sample* FindSample(const std::vector<Sample>& samples, const std::string& virtualPath)
    {
        for (const Sample& sample : samples)
        {
            if (sample.VirtualPath == virtualPath)
            {
                return &sample;
            }
        }
        return nullptr;
    }

    std::filesystem::path CreateSampleTree(std::vector<Sample>& outSamples)
    {
        std::error_code errorCode;
        std::filesystem::path root = std::filesystem::temp_directory_path(errorCode) / "HachimiTests";
        if (errorCode)
        {
            root = std::filesystem::current_path() / "HachimiTests";
        }
        root /= "samples";

        FileSystem::RemoveAll(root);
        FileSystem::CreateDirectories(root);

        const auto add = [&](const std::string& virtualPath, std::vector<uint8_t> content, bool expectStore)
        {
            Sample sample;
            sample.VirtualPath = virtualPath;
            sample.Content = std::move(content);
            sample.SourcePath = root / std::filesystem::path(virtualPath);
            sample.ExpectStore = expectStore;
            FileSystem::CreateDirectories(sample.SourcePath.parent_path());
            WriteFile(sample.SourcePath, sample.Content);
            outSamples.push_back(std::move(sample));
        };

        // Boundary cases around the block size drive the segmentation logic.
        add("Data/empty.bin", {}, false);
        add("Data/one_byte.bin", { 0x42 }, false);
        add("Data/exactly_one_block.bin", MakeCompressibleBytes(TestBlockSize), false);
        add("Data/block_plus_one.bin", MakeCompressibleBytes(TestBlockSize + 1), false);
        add(MultiBlockPath, MakeCompressibleBytes(TestBlockSize * 5 + 7), false);

        // Incompressible data must fall back to stored blocks inside a zstd entry.
        add(RandomPath, MakeRandomBytes(TestBlockSize * 4 + 3, 1337), false);
        // An already-compressed extension is stored outright.
        add("Assets/Textures/random.png", MakeRandomBytes(TestBlockSize * 2 + 11, 99), true);

        // Spaces and mixed case exercise path handling and lookup normalization.
        add("Assets/Meshes/Shared Meshes/Box Mesh.bin", MakeCompressibleBytes(1024), false);

        // Enough small text samples for the dictionary trainer to be exercised.
        for (int index = 0; index < 12; ++index)
        {
            const std::string name = "Assets/Scripts/script_" + std::to_string(index) + ".lua";
            const std::string text = "local M = {}\nfunction M:OnCreate()\n  HE.Log.Info(\"script "
                + std::to_string(index) + "\")\nend\nreturn M\n";
            add(name, std::vector<uint8_t>(text.begin(), text.end()), false);
        }

        return root;
    }

    void TestPackageRoundTrip(const std::filesystem::path& packagePath, const std::vector<Sample>& samples)
    {
        Section("package round trip");

        PackageReader reader;
        if (!reader.Open(packagePath))
        {
            Check(false, "reader failed to open the package it just wrote");
            return;
        }

        Check(reader.GetEntryCount() == samples.size() + 1,
            "entry count is " + std::to_string(reader.GetEntryCount()) + ", expected "
                + std::to_string(samples.size() + 1) + " including BuildInfo.yaml");

        std::printf("  memory mapped: %s, entries: %zu, dictionary: %u bytes\n",
            reader.IsMemoryMapped() ? "yes" : "no", reader.GetEntryCount(), reader.GetDictionarySize());

        for (const Sample& sample : samples)
        {
            const std::string label = sample.VirtualPath;

            size_t entryIndex = 0;
            if (!reader.FindEntry(sample.VirtualPath, entryIndex))
            {
                Check(false, "entry missing: " + label);
                continue;
            }

            const PackageEntryInfo& entry = reader.GetEntry(entryIndex);

            Check(entry.UncompressedSize == sample.Content.size(), label + ": uncompressed size mismatch");
            Check(entry.ContentHash == XXHash::Hash(sample.Content.data(), sample.Content.size()),
                label + ": content hash mismatch");
            Check(entry.BlockCount == ExpectedBlockCount(sample.Content.size()),
                label + ": block count is " + std::to_string(entry.BlockCount)
                    + ", expected " + std::to_string(ExpectedBlockCount(sample.Content.size())));
            Check(entry.CompressedSize <= entry.UncompressedSize,
                label + ": packed size exceeds the payload");

            if (sample.ExpectStore)
            {
                Check(entry.Method == PackageCompressionMethod::Store, label + ": expected a stored entry");
            }

            std::vector<uint8_t> readBack;
            Check(reader.ReadEntry(entryIndex, readBack), label + ": ReadEntry failed");
            Check(SameBytes(readBack, sample.Content), label + ": ReadEntry bytes differ");

            FileMapping mapping;
            Check(reader.MapEntry(entryIndex, mapping), label + ": MapEntry failed");
            Check(mapping.Size() == sample.Content.size(), label + ": MapEntry size differs");
            if (mapping.IsValid() && mapping.Size() == sample.Content.size())
            {
                const std::vector<uint8_t> mapped(mapping.Data(), mapping.Data() + mapping.Size());
                Check(SameBytes(mapped, sample.Content), label + ": MapEntry bytes differ");
            }

            if (!sample.Content.empty())
            {
                Check(mapping.IsValid() && mapping.Data() != nullptr, label + ": MapEntry returned no data");
            }

            Check(reader.VerifyEntry(entryIndex), label + ": VerifyEntry failed");
        }

        Section("range reads");
        {
            const Sample* sample = FindSample(samples, MultiBlockPath);
            size_t entryIndex = 0;
            if (sample != nullptr && reader.FindEntry(MultiBlockPath, entryIndex))
            {
                const size_t size = static_cast<size_t>(reader.GetEntry(entryIndex).UncompressedSize);
                bool allMatch = true;
                int rangeCount = 0;

                // Sweep every block boundary with lengths that straddle them.
                for (size_t offset = 0; offset < size && allMatch; offset += TestBlockSize)
                {
                    for (const size_t length : { size_t{ 1 }, size_t{ 4095 }, size_t{ 4096 }, size_t{ 5000 } })
                    {
                        std::vector<uint8_t> chunk;
                        if (!reader.ReadEntryRange(entryIndex, offset, length, chunk))
                        {
                            allMatch = false;
                            break;
                        }

                        const size_t expectedLength = std::min(length, size - offset);
                        if (chunk.size() != expectedLength || !SameSpan(chunk, sample->Content, offset))
                        {
                            allMatch = false;
                            break;
                        }
                        ++rangeCount;
                    }
                }

                // Reads past the end clamp instead of failing.
                std::vector<uint8_t> clamped;
                Check(reader.ReadEntryRange(entryIndex, size - 1, 9999, clamped) && clamped.size() == 1,
                    "a range read past the end was not clamped");
                Check(reader.ReadEntryRange(entryIndex, size + 5, 16, clamped) == false,
                    "a range read starting past the end was accepted");

                Check(allMatch, "range reads returned wrong bytes at a block boundary");
                std::printf("  %d range reads verified across %zu blocks\n", rangeCount,
                    static_cast<size_t>(reader.GetEntry(entryIndex).BlockCount));
            }
            else
            {
                Check(false, "range read fixture missing");
            }
        }

        Section("streaming reads");
        for (const std::string& path : { std::string(MultiBlockPath), std::string(RandomPath) })
        {
            size_t entryIndex = 0;
            if (!reader.FindEntry(path, entryIndex))
            {
                Check(false, path + ": streaming fixture missing");
                continue;
            }

            Scope<PackageEntryStream> stream = reader.OpenEntryStream(entryIndex);
            Check(stream != nullptr, path + ": OpenEntryStream failed");
            if (stream == nullptr)
            {
                continue;
            }

            const PackageEntryInfo& entry = reader.GetEntry(entryIndex);
            std::vector<uint8_t> streamed;
            streamed.reserve(static_cast<size_t>(entry.UncompressedSize));

            uint8_t buffer[777];
            for (;;)
            {
                const size_t read = stream->Read(buffer, sizeof(buffer));
                if (read == 0)
                {
                    break;
                }
                streamed.insert(streamed.end(), buffer, buffer + read);
            }

            Check(streamed.size() == entry.UncompressedSize, path + ": streamed length differs");

            const Sample* sample = FindSample(samples, path);
            if (sample != nullptr)
            {
                Check(SameBytes(streamed, sample->Content), path + ": streamed bytes differ from the source");
            }

            Check(stream->Seek(0) && stream->Read(buffer, 16) == 16, path + ": Seek back to start failed");
        }

        Section("verification");
        PackageVerificationReport report;
        Check(reader.VerifyAll(report), "VerifyAll reported failures on a pristine package");
        Check(report.FailedCount == 0, "VerifyAll reported " + std::to_string(report.FailedCount) + " failures");
        Check(report.EntryCount == reader.GetEntryCount(), "VerifyAll did not cover every entry");
    }

    void TestCorruption(const std::filesystem::path& packagePath, const std::filesystem::path& workDirectory)
    {
        Section("corruption handling");

        std::vector<uint8_t> pristine;
        Check(VirtualFileSystem::ReadBinaryFile(packagePath, pristine), "failed to read the package for corruption tests");

        const std::filesystem::path corruptDirectory = workDirectory / "corrupt";
        FileSystem::CreateDirectories(corruptDirectory);

        // Truncated package.
        {
            std::vector<uint8_t> bytes = pristine;
            bytes.resize(bytes.size() / 2);
            const std::filesystem::path truncated = corruptDirectory / "truncated.hpak";
            WriteFile(truncated, bytes);

            PackageReader reader;
            Check(!reader.Open(truncated), "a truncated package was accepted");
        }

        // Bad magic.
        {
            std::vector<uint8_t> bytes = pristine;
            bytes[0] = 'X';
            const std::filesystem::path badMagic = corruptDirectory / "bad_magic.hpak";
            WriteFile(badMagic, bytes);

            PackageReader reader;
            Check(!reader.Open(badMagic), "a package with a broken magic was accepted");
        }

        // A future format version must be rejected rather than misparsed.
        {
            std::vector<uint8_t> bytes = pristine;
            const uint32_t futureVersion = PackageFormat::Version + 1;
            std::memcpy(bytes.data() + offsetof(PackageFileHeader, FormatVersion), &futureVersion,
                sizeof(futureVersion));
            const std::filesystem::path futurePackage = corruptDirectory / "future_version.hpak";
            WriteFile(futurePackage, bytes);

            PackageReader reader;
            Check(!reader.Open(futurePackage), "a package with an unsupported version was accepted");
        }

        // A corrupted table of contents must be caught by its hash.
        {
            PackageReader probe;
            Check(probe.Open(packagePath), "probe reader failed to open the pristine package");
            const PackageFileHeader header = probe.GetHeader();
            probe.Close();

            std::vector<uint8_t> bytes = pristine;
            bytes[static_cast<size_t>(header.TocOffset) + sizeof(PackageTocHeader)] ^= 0xFF;
            const std::filesystem::path badToc = corruptDirectory / "bad_toc.hpak";
            WriteFile(badToc, bytes);

            PackageReader reader;
            Check(!reader.Open(badToc), "a package with a corrupted table of contents was accepted");
        }

        // A corrupted payload must still load, then fail verification for that
        // entry only. Random.bin is stored, so exactly one entry is affected.
        {
            PackageReader probe;
            probe.Open(packagePath);
            size_t entryIndex = 0;
            const bool found = probe.FindEntry(RandomPath, entryIndex);
            const uint64_t dataOffset = found ? probe.GetEntry(entryIndex).DataOffset : 0;
            probe.Close();

            Check(found, "probe could not find the corruption target");
            if (found)
            {
                std::vector<uint8_t> bytes = pristine;
                bytes[static_cast<size_t>(dataOffset) + 1] ^= 0xFF;
                const std::filesystem::path badPayload = corruptDirectory / "bad_payload.hpak";
                WriteFile(badPayload, bytes);

                PackageReader reader;
                Check(reader.Open(badPayload), "a package with a corrupted payload failed to load at all");

                size_t corruptedIndex = 0;
                if (reader.FindEntry(RandomPath, corruptedIndex))
                {
                    Check(!reader.VerifyEntry(corruptedIndex), "verification passed for a corrupted payload");
                }

                PackageVerificationReport report;
                reader.VerifyAll(report);
                Check(report.FailedCount >= 1, "VerifyAll did not report the corrupted entry");
                Check(report.FailedCount < report.EntryCount, "VerifyAll reported every entry as corrupt");
            }
        }
    }

    void TestVirtualFileSystem(const std::filesystem::path& packagePath, const std::vector<Sample>& samples)
    {
        Section("virtual file system");

        VirtualFileSystem::UnmountAll();
        const std::filesystem::path mountPoint = packagePath.parent_path();

        Check(VirtualFileSystem::MountArchive(packagePath, mountPoint), "failed to mount the package");
        Check(VirtualFileSystem::IsArchiveMounted(), "IsArchiveMounted returned false after mounting");
        Check(VirtualFileSystem::GetMounts().size() == 1, "mount table does not hold exactly one entry");
        Check(VirtualFileSystem::GetDataRoot() == std::filesystem::absolute(mountPoint).lexically_normal(),
            "GetDataRoot did not report the active mount point");

        // Packaged directories exist only as entry name prefixes.
        Check(VirtualFileSystem::Exists(mountPoint / "Data"), "'Data' should exist as a packaged directory");
        Check(VirtualFileSystem::IsDirectory(mountPoint / "Data"), "'Data' should be a directory");
        Check(!VirtualFileSystem::Exists(mountPoint / "Data/nope.bin"), "a missing entry reported as existing");
        Check(!VirtualFileSystem::IsPackagedPath(mountPoint / "Data/nope.bin"),
            "a missing path must not be reported as packaged");

        for (const Sample& sample : samples)
        {
            const std::filesystem::path virtualPath = mountPoint / std::filesystem::path(sample.VirtualPath);
            const std::string label = sample.VirtualPath;

            Check(VirtualFileSystem::Exists(virtualPath), label + ": VFS reports the entry missing");
            Check(VirtualFileSystem::IsPackagedPath(virtualPath), label + ": not resolved as packaged");
            Check(VirtualFileSystem::GetFileSize(virtualPath) == sample.Content.size(),
                label + ": VFS file size differs");

            std::vector<uint8_t> bytes;
            Check(VirtualFileSystem::ReadBinaryFile(virtualPath, bytes), label + ": VFS read failed");
            Check(SameBytes(bytes, sample.Content), label + ": VFS read bytes differ");

            FileMapping mapping;
            Check(VirtualFileSystem::MapFile(virtualPath, mapping), label + ": VFS map failed");
            Check(mapping.Size() == sample.Content.size(), label + ": VFS map size differs");

            if (sample.Content.size() > 8)
            {
                std::vector<uint8_t> range;
                Check(VirtualFileSystem::ReadFileRange(virtualPath, 3, 4, range), label + ": VFS range read failed");
                Check(range.size() == 4 && SameSpan(range, sample.Content, 3), label + ": VFS range bytes differ");
            }
        }

        Section("lookup normalization");
        // Archive lookups follow Windows path semantics, so casing must not matter.
        Check(VirtualFileSystem::Exists(mountPoint / "data/MULTI_BLOCK.BIN"),
            "case-insensitive lookup failed");
        Check(VirtualFileSystem::Exists(mountPoint / "./Data/one_byte.bin"),
            "a './' prefixed lookup failed");
        Check(!VirtualFileSystem::Exists(mountPoint / "Data/../../escape.bin"),
            "a path escaping the mount point resolved to something");

        Section("directory enumeration");
        size_t expectedRootFiles = 0;
        for (const Sample& sample : samples)
        {
            if (std::filesystem::path(sample.VirtualPath).parent_path() == std::filesystem::path("Data"))
            {
                ++expectedRootFiles;
            }
        }

        const std::vector<std::filesystem::path> rootFiles = VirtualFileSystem::GetFiles(mountPoint / "Data");
        Check(rootFiles.size() == expectedRootFiles,
            "Data should list " + std::to_string(expectedRootFiles) + " files, found "
                + std::to_string(rootFiles.size()));
        Check(std::is_sorted(rootFiles.begin(), rootFiles.end()), "GetFiles results are not sorted");

        for (const std::filesystem::path& file : rootFiles)
        {
            Check(file.parent_path() == mountPoint / "Data",
                "GetFiles returned a path outside the requested directory: " + file.string());
        }

        // Recursive listing inside the package namespaces, where no disk content
        // can shadow the archive.
        const std::vector<std::filesystem::path> recursiveData =
            VirtualFileSystem::GetFilesRecursive(mountPoint / "Data");
        Check(recursiveData.size() == expectedRootFiles,
            "recursive Data listing found " + std::to_string(recursiveData.size()) + " files, expected "
                + std::to_string(expectedRootFiles));

        const std::vector<std::filesystem::path> recursiveAssets =
            VirtualFileSystem::GetFilesRecursive(mountPoint / "Assets");
        Check(recursiveAssets.size() == samples.size() - expectedRootFiles,
            "recursive Assets listing found " + std::to_string(recursiveAssets.size()) + " files, expected "
                + std::to_string(samples.size() - expectedRootFiles));

        // At the mount point the archive is merged with any loose files sitting
        // next to the package, so the listing is a superset of the archive.
        const std::vector<std::filesystem::path> recursiveRoot = VirtualFileSystem::GetFilesRecursive(mountPoint);
        Check(recursiveRoot.size() >= samples.size() + 1,
            "recursive listing lost packaged entries: found " + std::to_string(recursiveRoot.size()));

        const std::vector<std::filesystem::path> scripts = VirtualFileSystem::GetFiles(mountPoint / "Assets/Scripts");
        Check(scripts.size() == 12, "Assets/Scripts should list 12 files, found " + std::to_string(scripts.size()));

        Section("priority overlay");
        const std::filesystem::path overlayDirectory = mountPoint.parent_path() / "overlay_content";
        FileSystem::RemoveAll(overlayDirectory);
        FileSystem::CreateDirectories(overlayDirectory / "Data");
        const std::vector<uint8_t> overlayBytes { 'o', 'v', 'e', 'r', 'l', 'a', 'y' };
        WriteFile(overlayDirectory / "Data" / "one_byte.bin", overlayBytes);

        Check(VirtualFileSystem::MountDirectory(overlayDirectory, mountPoint, 10), "failed to mount the overlay");
        Check(VirtualFileSystem::GetMounts().size() == 2, "the overlay did not stack on the package");
        Check(VirtualFileSystem::GetMounts().front().Priority == 10, "mounts are not ordered by priority");
        {
            std::vector<uint8_t> shadowed;
            Check(VirtualFileSystem::ReadBinaryFile(mountPoint / "Data/one_byte.bin", shadowed),
                "overlay read failed");
            Check(SameBytes(shadowed, overlayBytes), "the higher priority mount did not shadow the package");

            std::vector<uint8_t> untouched;
            Check(VirtualFileSystem::ReadBinaryFile(mountPoint / "Data/empty.bin", untouched),
                "package entry unreadable while an overlay is mounted");
            Check(untouched.empty(), "an overlay leaked into an unrelated entry");
        }

        VirtualFileSystem::UnmountAll();
        Check(!VirtualFileSystem::IsArchiveMounted(), "IsArchiveMounted returned true after UnmountAll");
        Check(VirtualFileSystem::GetMounts().empty(), "mount table is not empty after UnmountAll");
        FileSystem::RemoveAll(overlayDirectory);

        Section("disk fallback");
        // With nothing mounted the VFS must behave exactly like direct disk access.
        const Sample* first = samples.empty() ? nullptr : &samples.front();
        if (first != nullptr)
        {
            std::vector<uint8_t> viaVfs;
            Check(VirtualFileSystem::ReadBinaryFile(first->SourcePath, viaVfs), "unmounted VFS read failed");
            Check(SameBytes(viaVfs, first->Content), "unmounted VFS read differs from the file on disk");
            Check(VirtualFileSystem::GetFileSize(first->SourcePath) == FileSystem::GetFileSize(first->SourcePath),
                "unmounted VFS file size differs from FileSystem");
            Check(VirtualFileSystem::GetFiles(first->SourcePath.parent_path()).size()
                    == FileSystem::GetFiles(first->SourcePath.parent_path()).size(),
                "unmounted VFS listing differs from FileSystem");
        }
    }

    void TestAsyncReads(const std::filesystem::path& packagePath, const std::vector<Sample>& samples)    {
        Section("asynchronous reads");

        VirtualFileSystem::UnmountAll();
        const std::filesystem::path mountPoint = packagePath.parent_path();
        Check(VirtualFileSystem::MountArchive(packagePath, mountPoint), "failed to mount for async reads");

        const Sample* sample = FindSample(samples, MultiBlockPath);
        Check(sample != nullptr, "async fixture missing");

        int callbackCount = 0;
        bool asyncSuccess = false;
        std::vector<uint8_t> asyncBytes;

        const uint64_t requestId = VirtualFileSystem::ReadFileAsync(mountPoint / MultiBlockPath,
            [&](bool success, std::vector<uint8_t>&& data)
            {
                ++callbackCount;
                asyncSuccess = success;
                asyncBytes = std::move(data);
            });

        Check(requestId != 0, "ReadFileAsync returned no request id");

        for (int spin = 0; spin < 2000 && callbackCount == 0; ++spin)
        {
            VirtualFileSystem::PumpCompletedRequests();
            if (callbackCount == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        Check(callbackCount == 1, "async callback ran " + std::to_string(callbackCount) + " times");
        Check(asyncSuccess, "async read reported failure");
        if (sample != nullptr)
        {
            Check(SameBytes(asyncBytes, sample->Content), "async read bytes differ");
        }
        Check(VirtualFileSystem::GetPendingRequestCount() == 0, "a request stayed pending after pumping");

        // A cancelled request must never deliver its callback.
        int cancelledCallbacks = 0;
        const uint64_t cancelledId = VirtualFileSystem::ReadFileAsync(mountPoint / MultiBlockPath,
            [&](bool, std::vector<uint8_t>&&) { ++cancelledCallbacks; });
        Check(VirtualFileSystem::IsRequestPending(cancelledId), "a fresh request is not reported as pending");
        VirtualFileSystem::CancelRequest(cancelledId);
        for (int spin = 0; spin < 400 && cancelledCallbacks == 0; ++spin)
        {
            VirtualFileSystem::PumpCompletedRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(cancelledCallbacks == 0, "a cancelled request still invoked its callback");

        // Unmounting drops outstanding work without invoking callbacks.
        int droppedCallbacks = 0;
        VirtualFileSystem::ReadFileAsync(mountPoint / MultiBlockPath,
            [&](bool, std::vector<uint8_t>&&) { ++droppedCallbacks; });
        VirtualFileSystem::UnmountAll();
        for (int spin = 0; spin < 200; ++spin)
        {
            VirtualFileSystem::PumpCompletedRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(droppedCallbacks == 0, "a request invoked its callback after UnmountAll");
        Check(VirtualFileSystem::GetPendingRequestCount() == 0, "requests survived UnmountAll");
    }
}

// Writes a package laid out exactly like ProjectPackager produces one, so the
// Player's headless modes can be verified against a real artifact.
int EmitPackage(const std::filesystem::path& packagePath)
{
    const std::string buildInfo =
        "ProductName: Hachimi Smoke Test\n"
        "StartScene: Assets/Scenes/Default.hscene\n"
        "WindowWidth: 1280\n"
        "WindowHeight: 720\n"
        "VSync: true\n";

    const std::string project =
        "Project: SmokeTest\n"
        "ProjectDirectory: .\n"
        "AssetsDirectory: Assets\n"
        "StartScene: Assets/Scenes/Default.hscene\n";

    const std::string scene =
        "Scene: Default\n"
        "Environment:\n"
        "  ShowSkybox: true\n"
        "  Exposure: 1\n"
        "  EnvironmentIntensity: 1\n"
        "Physics:\n"
        "  Gravity: [0, -9.81, 0]\n"
        "  FixedTimeStep: 0.0166667\n"
        "  SubStepCount: 4\n"
        "  EnableSleep: true\n"
        "  EnableContinuous: true\n"
        "Entities: []\n";

    const std::string script =
        "local M = {}\n"
        "function M:OnCreate()\n"
        "  HE.Log.Info(\"smoke test\")\n"
        "end\n"
        "function M:OnUpdate(deltaTime)\n"
        "end\n"
        "return M\n";

    PackageWriter writer;
    if (!writer.Open(packagePath))
    {
        std::printf("Failed to open '%s' for writing\n", packagePath.string().c_str());
        return 1;
    }

    writer.AddMemory("BuildInfo.yaml", buildInfo.data(), buildInfo.size());
    writer.AddMemory("Project.hproj", project.data(), project.size());
    writer.AddMemory("Assets/Scenes/Default.hscene", scene.data(), scene.size());
    writer.AddMemory("Assets/Scripts/Rotator.lua", script.data(), script.size());

    // Package the real engine runtime files when they sit next to this executable,
    // which is how the editor's exporter finds them.
    const std::filesystem::path exeDirectory = PlatformUtils::GetExecutableDirectory();
    size_t runtimeFileCount = 0;
    for (const std::filesystem::path& shader : FileSystem::GetFiles(exeDirectory / "Shaders"))
    {
        writer.AddFile(std::filesystem::path("Shaders") / FileSystem::GetFileName(shader), shader);
        ++runtimeFileCount;
    }
    for (const std::filesystem::path& font : FileSystem::GetFiles(exeDirectory / "Assets" / "Fonts"))
    {
        writer.AddFile(std::filesystem::path("Assets") / "Fonts" / FileSystem::GetFileName(font), font);
        ++runtimeFileCount;
    }

    // Synthetic texture bytes give the package a stored (already compressed) entry.
    const std::vector<uint8_t> texture = MakeRandomBytes(4096 * 3 + 5, 7);
    writer.AddMemory("Assets/Textures/Grid.png", texture.data(), texture.size());

    PackageBuildReport report;
    if (!writer.Finalize(report))
    {
        std::printf("Failed to finalize '%s'\n", packagePath.string().c_str());
        return 1;
    }

    std::printf("Wrote %s\n  entries: %u (%u stored, %u zstd), blocks: %u, dictionary: %u\n"
        "  raw: %llu bytes, packed: %llu bytes, runtime files from %s: %zu\n",
        packagePath.string().c_str(),
        report.EntryCount, report.StoredEntryCount, report.ZstdEntryCount, report.BlockCount,
        report.DictionarySize,
        static_cast<unsigned long long>(report.UncompressedBytes),
        static_cast<unsigned long long>(report.CompressedBytes),
        exeDirectory.string().c_str(),
        runtimeFileCount);
    return 0;
}

int main(int argc, char** argv)
{
    HE::Log::Init();
    HE::JobSystem::Init();
    HE::VirtualFileSystem::ResetStatistics();

    // Package-emitting mode: produces an artifact for the Player's headless modes.
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument(argv[index]);
        const std::string prefix = "--emit-package=";
        if (argument.starts_with(prefix))
        {
            const std::filesystem::path target = argument.substr(prefix.size());
            FileSystem::CreateDirectories(target.parent_path());
            const int result = EmitPackage(target);
            JobSystem::Shutdown();
            Log::Shutdown();
            return result;
        }
    }

    // The reason this refactor exists: reading a package must never extract it.
    const std::filesystem::path extractionCache = HE::PlatformUtils::GetLocalAppDataDirectory() / "Packages";
    const bool cacheExistedBefore = HE::FileSystem::Exists(extractionCache);

    std::error_code errorCode;
    std::filesystem::path workDirectory = std::filesystem::temp_directory_path(errorCode) / "HachimiTests";
    if (errorCode)
    {
        workDirectory = std::filesystem::current_path() / "HachimiTests";
    }
    HE::FileSystem::RemoveAll(workDirectory);
    HE::FileSystem::CreateDirectories(workDirectory);

    std::vector<Sample> samples;
    const std::filesystem::path sampleRoot = CreateSampleTree(samples);

    HE::PackageWriterSettings settings;
    settings.BlockSize = TestBlockSize;
    settings.CompressionLevel = 3;
    settings.UseDictionary = true;
    settings.MultiThreaded = true;

    const std::filesystem::path packagePath = workDirectory / "Test.hpak";
    const std::string buildInfo = "ProductName: RoundTrip\nStartScene: Data/multi_block.bin\n"
        "WindowWidth: 800\nWindowHeight: 600\nVSync: true\n";

    Section("package writing");
    {
        HE::PackageWriter writer;
        Check(writer.Open(packagePath, settings), "PackageWriter::Open failed");

        for (const Sample& sample : samples)
        {
            Check(writer.AddFile(sample.VirtualPath, sample.SourcePath),
                "AddFile failed for " + sample.VirtualPath);
        }

        Check(writer.AddMemory("BuildInfo.yaml", buildInfo.data(), buildInfo.size()),
            "AddMemory failed for BuildInfo.yaml");
        Check(!writer.AddFile("Data/../escape.bin", samples.front().SourcePath),
            "AddFile accepted a path that escapes the package root");
        Check(!writer.AddMemory("/absolute.bin", buildInfo.data(), buildInfo.size()),
            "AddMemory accepted an absolute entry path");
        Check(writer.GetEntryCount() == samples.size() + 1,
            "staged entry count is " + std::to_string(writer.GetEntryCount()));

        HE::PackageBuildReport report;
        Check(writer.Finalize(report), "PackageWriter::Finalize failed");
        Check(report.EntryCount == samples.size() + 1,
            "report entry count is " + std::to_string(report.EntryCount));
        Check(report.UncompressedBytes > 0, "report has no uncompressed bytes");
        Check(report.CompressedBytes > 0, "report has no compressed bytes");
        Check(report.CompressedBytes <= report.UncompressedBytes, "packing made the payload larger");
        Check(!writer.IsOpen(), "the writer stayed open after Finalize");

        std::printf("  entries: %u (%u stored, %u zstd), blocks: %u\n",
            report.EntryCount, report.StoredEntryCount, report.ZstdEntryCount, report.BlockCount);
        std::printf("  raw: %llu bytes, packed: %llu bytes (%.1f%%), dictionary: %u bytes, %.3fs\n",
            static_cast<unsigned long long>(report.UncompressedBytes),
            static_cast<unsigned long long>(report.CompressedBytes),
            report.CompressionRatio() * 100.0,
            report.DictionarySize,
            report.Seconds);
    }

    TestPackageRoundTrip(packagePath, samples);

    Section("BuildInfo entry");
    {
        Check(HE::VirtualFileSystem::MountArchive(packagePath, packagePath.parent_path()), "mount failed");
        std::string text;
        Check(HE::VirtualFileSystem::ReadTextFile(packagePath.parent_path() / "BuildInfo.yaml", text),
            "failed to read BuildInfo.yaml through the VFS");
        HE::PackageBuildInfo parsed;
        Check(HE::ParsePackageBuildInfo(text, parsed), "failed to parse BuildInfo.yaml");
        Check(parsed.ProductName == "RoundTrip", "BuildInfo ProductName differs");
        Check(parsed.WindowWidth == 800, "BuildInfo WindowWidth differs");
        Check(parsed.StartScene == std::filesystem::path("Data/multi_block.bin"), "BuildInfo StartScene differs");
        HE::VirtualFileSystem::UnmountAll();
    }

    Section("reproducible output");
    {
        const std::filesystem::path second = workDirectory / "Test2.hpak";
        HE::PackageWriter writer;
        Check(writer.Open(second, settings), "second writer Open failed");
        for (const Sample& sample : samples)
        {
            writer.AddFile(sample.VirtualPath, sample.SourcePath);
        }
        writer.AddMemory("BuildInfo.yaml", buildInfo.data(), buildInfo.size());

        HE::PackageBuildReport report;
        Check(writer.Finalize(report), "second Finalize failed");

        HE::PackageReader firstReader;
        HE::PackageReader secondReader;
        Check(firstReader.Open(packagePath) && secondReader.Open(second), "failed to reopen both packages");
        Check(firstReader.GetPackageId() == secondReader.GetPackageId(),
            "identical inputs produced different package ids");
        Check(HE::FileSystem::GetFileSize(packagePath) == HE::FileSystem::GetFileSize(second),
            "identical inputs produced different package sizes");
    }

    TestCorruption(packagePath, workDirectory);
    TestVirtualFileSystem(packagePath, samples);
    TestAsyncReads(packagePath, samples);

    Section("no extraction");
    {
        HE::VirtualFileSystem::UnmountAll();
        const HE::VirtualFileSystem::Statistics statistics = HE::VirtualFileSystem::GetStatistics();
        std::printf("  reads: %llu, bytes: %llu, archive: %llu, disk: %llu, async: %llu\n",
            static_cast<unsigned long long>(statistics.ReadCount),
            static_cast<unsigned long long>(statistics.BytesRead),
            static_cast<unsigned long long>(statistics.ArchiveReadCount),
            static_cast<unsigned long long>(statistics.DiskReadCount),
            static_cast<unsigned long long>(statistics.AsyncRequestCount));

        Check(statistics.ArchiveReadCount > 0, "no reads were served from a package");
        if (!cacheExistedBefore)
        {
            Check(!HE::FileSystem::Exists(extractionCache),
                "reading a package created an extraction cache at " + extractionCache.string());
        }
        else
        {
            std::printf("  (extraction cache already existed before the run; creation check skipped)\n");
        }
    }

    HE::FileSystem::RemoveAll(workDirectory);
    HE::FileSystem::RemoveAll(sampleRoot);

    HE::JobSystem::Shutdown();
    HE::Log::Shutdown();

    std::printf("\n%d checks, %d failures\n", g_CheckCount, g_FailureCount);
    return g_FailureCount == 0 ? 0 : 1;
}
