// PackageReader corruption handling: broken header, table of contents and payload.

#include <doctest/doctest.h>

#include "Packaging/PackageFormat.h"
#include "Packaging/PackageReader.h"
#include "Support/TestWorkspace.h"
#include "Utils/VirtualFileSystem.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    std::vector<uint8_t> ReadPackageBytes(const std::filesystem::path& path)
    {
        std::vector<uint8_t> bytes;
        if (!VirtualFileSystem::ReadBinaryFile(path, bytes))
        {
            return {};
        }
        return bytes;
    }

    std::filesystem::path WriteCorruptCopy(const std::string& name, const std::vector<uint8_t>& bytes)
    {
        const std::filesystem::path path = Workspace().PrepareDirectory("corrupt") / name;
        WriteBytes(path, bytes);
        return path;
    }
}

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("a truncated package is rejected")
{
    std::vector<uint8_t> bytes = ReadPackageBytes(Workspace().PackagePath());
    REQUIRE_FALSE(bytes.empty());

    bytes.resize(bytes.size() / 2);

    PackageReader reader;
    CHECK_FALSE(reader.Open(WriteCorruptCopy("truncated.hpak", bytes)));
    CHECK_FALSE(reader.IsOpen());
}

TEST_CASE("a package with a broken magic is rejected")
{
    std::vector<uint8_t> bytes = ReadPackageBytes(Workspace().PackagePath());
    REQUIRE_FALSE(bytes.empty());

    bytes[0] = 'X';

    PackageReader reader;
    CHECK_FALSE(reader.Open(WriteCorruptCopy("bad_magic.hpak", bytes)));
}

TEST_CASE("a package from a future format version is rejected")
{
    std::vector<uint8_t> bytes = ReadPackageBytes(Workspace().PackagePath());
    REQUIRE_FALSE(bytes.empty());

    const uint32_t futureVersion = PackageFormat::Version + 1;
    std::memcpy(bytes.data() + offsetof(PackageFileHeader, FormatVersion), &futureVersion,
        sizeof(futureVersion));

    PackageReader reader;
    CHECK_FALSE(reader.Open(WriteCorruptCopy("future_version.hpak", bytes)));
}

TEST_CASE("a corrupted table of contents is rejected")
{
    const std::filesystem::path pristinePath = Workspace().PackagePath();

    PackageFileHeader header {};
    {
        PackageReader probe;
        REQUIRE(probe.Open(pristinePath));
        header = probe.GetHeader();
    }

    std::vector<uint8_t> bytes = ReadPackageBytes(pristinePath);
    REQUIRE_FALSE(bytes.empty());

    const size_t offset = static_cast<size_t>(header.TocOffset) + sizeof(PackageTocHeader);
    REQUIRE(offset < bytes.size());
    bytes[offset] ^= 0xFF;

    PackageReader reader;
    CHECK_FALSE(reader.Open(WriteCorruptCopy("bad_toc.hpak", bytes)));
}

TEST_CASE("a corrupted payload fails verification for that entry only")
{
    const std::filesystem::path pristinePath = Workspace().PackagePath();

    // Random.bin is stored, so flipping one of its bytes affects exactly one entry.
    uint64_t dataOffset = 0;
    {
        PackageReader probe;
        REQUIRE(probe.Open(pristinePath));

        size_t entryIndex = 0;
        REQUIRE(probe.FindEntry(RandomEntry, entryIndex));
        dataOffset = probe.GetEntry(entryIndex).DataOffset;
    }

    std::vector<uint8_t> bytes = ReadPackageBytes(pristinePath);
    REQUIRE_FALSE(bytes.empty());
    REQUIRE(static_cast<size_t>(dataOffset) + 1 < bytes.size());
    bytes[static_cast<size_t>(dataOffset) + 1] ^= 0xFF;

    const std::filesystem::path corruptPath = WriteCorruptCopy("bad_payload.hpak", bytes);

    PackageReader reader;
    CHECK(reader.Open(corruptPath));

    size_t corruptedIndex = 0;
    if (reader.FindEntry(RandomEntry, corruptedIndex))
    {
        CHECK_FALSE(reader.VerifyEntry(corruptedIndex));
    }
    else
    {
        FAIL_CHECK("the corruption target vanished from the package");
    }

    PackageVerificationReport report;
    reader.VerifyAll(report);
    CHECK(report.FailedCount >= 1);
    CHECK(report.FailedCount < report.EntryCount);
}

TEST_SUITE_END();
