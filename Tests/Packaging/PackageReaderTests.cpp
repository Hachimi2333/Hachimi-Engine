// PackageReader: entry round trips, range reads, streaming reads and verification.

#include <doctest/doctest.h>

#include "Packaging/PackageEntryStream.h"
#include "Packaging/PackageFormat.h"
#include "Packaging/PackageReader.h"
#include "Support/TestWorkspace.h"
#include "Utils/XXHash.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("every entry round trips through the reader")
{
    const TestWorkspace& workspace = Workspace();
    const std::vector<Sample>& samples = workspace.Samples();

    PackageReader reader;
    REQUIRE(reader.Open(workspace.PackagePath()));
    CHECK(reader.GetEntryCount() == samples.size() + 1); // + BuildInfo.yaml

    MESSAGE("memory mapped: ", std::string(reader.IsMemoryMapped() ? "yes" : "no"), ", entries: ",
        reader.GetEntryCount(), ", dictionary: ", reader.GetDictionarySize(), " bytes");

    for (const Sample& sample : samples)
    {
        INFO("entry: ", sample.VirtualPath);

        size_t entryIndex = 0;
        if (!reader.FindEntry(sample.VirtualPath, entryIndex))
        {
            FAIL_CHECK("entry is missing from the package");
            continue;
        }

        const PackageEntryInfo& entry = reader.GetEntry(entryIndex);
        CHECK(entry.UncompressedSize == sample.Content.size());
        CHECK(entry.ContentHash == XXHash::Hash(sample.Content.data(), sample.Content.size()));
        CHECK(entry.BlockCount == ExpectedBlockCount(sample.Content.size()));
        CHECK(entry.CompressedSize <= entry.UncompressedSize);
        if (sample.ExpectStored)
        {
            CHECK(entry.Method == PackageCompressionMethod::Store);
        }

        std::vector<uint8_t> readBack;
        if (!reader.ReadEntry(entryIndex, readBack))
        {
            FAIL_CHECK("ReadEntry failed");
        }
        else
        {
            CHECK(SameBytes(readBack, sample.Content));
        }

        FileMapping mapping;
        if (!reader.MapEntry(entryIndex, mapping))
        {
            FAIL_CHECK("MapEntry failed");
        }
        else
        {
            CHECK(mapping.Size() == sample.Content.size());
            if (mapping.IsValid() && mapping.Size() == sample.Content.size())
            {
                const std::vector<uint8_t> mapped(mapping.Data(), mapping.Data() + mapping.Size());
                CHECK(SameBytes(mapped, sample.Content));
            }
            if (!sample.Content.empty())
            {
                CHECK(mapping.Data() != nullptr);
            }
        }

        CHECK(reader.VerifyEntry(entryIndex));
    }
}

TEST_CASE("the table of contents covers BuildInfo.yaml and verifies clean")
{
    const TestWorkspace& workspace = Workspace();

    PackageReader reader;
    REQUIRE(reader.Open(workspace.PackagePath()));

    size_t entryIndex = 0;
    REQUIRE(reader.FindEntry("BuildInfo.yaml", entryIndex));

    std::vector<uint8_t> bytes;
    REQUIRE(reader.ReadEntry(entryIndex, bytes));
    const std::string text(bytes.begin(), bytes.end());

    PackageBuildInfo buildInfo;
    REQUIRE(ParsePackageBuildInfo(text, buildInfo));
    CHECK(buildInfo.IsValid());
    CHECK(buildInfo.ProductName == "RoundTrip");
    CHECK(buildInfo.StartScene == std::filesystem::path(MultiBlockEntry));
    CHECK(buildInfo.WindowWidth == 800u);
    CHECK(buildInfo.WindowHeight == 600u);
    CHECK(buildInfo.VSync == true);

    PackageVerificationReport report;
    CHECK(reader.VerifyAll(report));
    CHECK(report.FailedCount == 0);
    CHECK(report.EntryCount == reader.GetEntryCount());
    CHECK(report.VerifiedCount == report.EntryCount);
}

TEST_CASE("range reads follow the block boundaries")
{
    const TestWorkspace& workspace = Workspace();
    const Sample* sample = workspace.FindSample(MultiBlockEntry);
    REQUIRE(sample != nullptr);

    PackageReader reader;
    REQUIRE(reader.Open(workspace.PackagePath()));

    size_t entryIndex = 0;
    REQUIRE(reader.FindEntry(MultiBlockEntry, entryIndex));

    const size_t size = static_cast<size_t>(reader.GetEntry(entryIndex).UncompressedSize);
    REQUIRE(size == sample->Content.size());

    // Sweep every block boundary with lengths that straddle it.
    int rangeCount = 0;
    bool allMatch = true;
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

    CHECK(allMatch);
    CHECK(rangeCount > 0);
    MESSAGE(rangeCount, " range reads verified across ", reader.GetEntry(entryIndex).BlockCount,
        " blocks");

    // Reads past the end clamp instead of failing.
    std::vector<uint8_t> clamped;
    CHECK(reader.ReadEntryRange(entryIndex, size - 1, 9999, clamped));
    REQUIRE(clamped.size() == 1);
    CHECK(clamped[0] == sample->Content.back());
    CHECK_FALSE(reader.ReadEntryRange(entryIndex, size + 5, 16, clamped));
}

TEST_CASE("streaming reads match the source bytes")
{
    const TestWorkspace& workspace = Workspace();

    PackageReader reader;
    REQUIRE(reader.Open(workspace.PackagePath()));

    for (const char* entryName : { MultiBlockEntry, RandomEntry })
    {
        INFO("entry: ", entryName);

        const Sample* sample = workspace.FindSample(entryName);
        REQUIRE(sample != nullptr);

        size_t entryIndex = 0;
        REQUIRE(reader.FindEntry(entryName, entryIndex));

        Scope<PackageEntryStream> stream = reader.OpenEntryStream(entryIndex);
        REQUIRE(stream != nullptr);

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

        CHECK(streamed.size() == entry.UncompressedSize);
        CHECK(SameBytes(streamed, sample->Content));

        CHECK(stream->Seek(0));
        CHECK(stream->Read(buffer, 16) == 16);
    }
}

TEST_SUITE_END();
