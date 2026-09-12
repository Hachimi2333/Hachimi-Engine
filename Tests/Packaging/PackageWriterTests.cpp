// PackageWriter: staged entries, entry-name validation and the build report.

#include <doctest/doctest.h>

#include "Packaging/PackageReader.h"
#include "Packaging/PackageWriter.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <cstdint>
#include <filesystem>
#include <string>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    PackageWriterSettings MakeWriterSettings()
    {
        PackageWriterSettings settings;
        settings.BlockSize = TestBlockSize;
        settings.CompressionLevel = 3;
        settings.UseDictionary = true;
        settings.MultiThreaded = true;
        return settings;
    }
}

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("PackageWriter stages entries and reports the build")
{
    const TestWorkspace& workspace = Workspace();
    const std::filesystem::path packagePath = workspace.PrepareDirectory("writer") / "Writer.hpak";
    const size_t expectedEntries = workspace.Samples().size() + 1; // + BuildInfo.yaml

    PackageWriter writer;
    REQUIRE(writer.Open(packagePath, MakeWriterSettings()));

    for (const Sample& sample : workspace.Samples())
    {
        INFO("entry: ", sample.VirtualPath);
        CHECK(writer.AddFile(sample.VirtualPath, sample.SourcePath));
    }

    const std::string& buildInfo = workspace.BuildInfoYaml();
    CHECK(writer.AddMemory("BuildInfo.yaml", buildInfo.data(), buildInfo.size()));

    // Paths a package must never accept: escaping the root and absolute paths.
    const Sample& first = workspace.Samples().front();
    CHECK_FALSE(writer.AddFile("Data/../escape.bin", first.SourcePath));
    CHECK_FALSE(writer.AddMemory("/absolute.bin", buildInfo.data(), buildInfo.size()));

    CHECK(writer.GetEntryCount() == expectedEntries);

    PackageBuildReport report;
    REQUIRE(writer.Finalize(report));

    CHECK(report.EntryCount == static_cast<uint32_t>(expectedEntries));
    CHECK(report.UncompressedBytes > 0);
    CHECK(report.CompressedBytes > 0);
    CHECK(report.CompressedBytes <= report.UncompressedBytes);
    CHECK(report.BlockCount > 0);
    CHECK_FALSE(writer.IsOpen());
    CHECK(FileSystem::GetFileSize(packagePath) > 0);

    MESSAGE("entries: ", report.EntryCount, " (", report.StoredEntryCount, " stored, ",
        report.ZstdEntryCount, " zstd), blocks: ", report.BlockCount, ", dictionary: ",
        report.DictionarySize, " bytes, ratio: ", report.CompressionRatio());
}

TEST_CASE("identical inputs produce identical packages")
{
    const TestWorkspace& workspace = Workspace();
    const std::filesystem::path directory = workspace.PrepareDirectory("reproducible");
    const std::filesystem::path firstPath = directory / "First.hpak";
    const std::filesystem::path secondPath = directory / "Second.hpak";

    const auto write = [&workspace](const std::filesystem::path& path, PackageBuildReport& outReport)
    {
        PackageWriter writer;
        REQUIRE(writer.Open(path, MakeWriterSettings()));

        for (const Sample& sample : workspace.Samples())
        {
            REQUIRE(writer.AddFile(sample.VirtualPath, sample.SourcePath));
        }

        const std::string& buildInfo = workspace.BuildInfoYaml();
        REQUIRE(writer.AddMemory("BuildInfo.yaml", buildInfo.data(), buildInfo.size()));
        REQUIRE(writer.Finalize(outReport));
    };

    PackageBuildReport firstReport;
    PackageBuildReport secondReport;
    write(firstPath, firstReport);
    write(secondPath, secondReport);

    PackageReader firstReader;
    PackageReader secondReader;
    REQUIRE(firstReader.Open(firstPath));
    REQUIRE(secondReader.Open(secondPath));

    CHECK(firstReader.GetPackageId() == secondReader.GetPackageId());
    CHECK(firstReport.PackageId == secondReport.PackageId);
    CHECK(firstReport.PackageId == firstReader.GetPackageId());
    CHECK(FileSystem::GetFileSize(firstPath) == FileSystem::GetFileSize(secondPath));
}

TEST_SUITE_END();
