// PackageFormat: the pure helpers and layout guarantees every reader and writer shares.

#include <doctest/doctest.h>

#include "Packaging/PackageFormat.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>

using namespace HachimiEngine;

namespace
{
    // Layout guarantees the on-disk format relies on; changing one is a format change.
    static_assert(PackageFormat::Version == 2);
    static_assert(PackageFormat::DefaultBlockSize == 256u * 1024u);
    static_assert(PackageFormat::BlockAlignment == 16);
    static_assert(PackageFormat::DefaultCompressionLevel == 3);
    static_assert(sizeof(PackageFileHeader) == 72);
    static_assert(sizeof(PackageTocHeader) == 24);
    static_assert(sizeof(PackageEntryRecord) == 64);
    static_assert(sizeof(PackageBlockRecord) == 8);
    static_assert(sizeof(PackageFooter) == 32);
}

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("the package magic identifies both ends of the file")
{
    const uint8_t expectedMagic[8] = { 'H', 'E', 'P', 'A', 'K', '2', '\r', '\n' };
    const uint8_t expectedFooterMagic[8] = { 'H', 'E', 'P', 'A', 'K', 'E', 'N', 'D' };

    CHECK(std::equal(std::begin(PackageFormat::Magic), std::end(PackageFormat::Magic), expectedMagic));
    CHECK(std::equal(std::begin(PackageFormat::FooterMagic), std::end(PackageFormat::FooterMagic),
        expectedFooterMagic));
}

TEST_CASE("MakePackageEntryName canonicalizes entry paths")
{
    CHECK(MakePackageEntryName(std::filesystem::path("Data/x.bin")) == "Data/x.bin");
    CHECK(MakePackageEntryName(std::filesystem::path("Data\\x.bin")) == "Data/x.bin");
    CHECK(MakePackageEntryName(std::filesystem::path("./Data/x.bin")) == "Data/x.bin");
    CHECK(MakePackageEntryName(std::filesystem::path("././Data/x.bin")) == "Data/x.bin");
    CHECK(MakePackageEntryName(std::filesystem::path("Assets/Meshes/Shared Meshes/Box Mesh.bin"))
        == "Assets/Meshes/Shared Meshes/Box Mesh.bin");

    // Anything that must never enter a package resolves to an empty name.
    for (const char* rejected : { "", "/absolute.bin", "C:/x.bin", "..", "../x.bin",
             "Data/../escape.bin", "Data/../../escape.bin", "Data/", "Data//x.bin" })
    {
        INFO("path: ", rejected);
        CHECK(MakePackageEntryName(std::filesystem::path(rejected)).empty());
    }
}

TEST_CASE("IsSafePackageEntryName rejects anything that could escape the package")
{
    CHECK(IsSafePackageEntryName("x.bin"));
    CHECK(IsSafePackageEntryName("Data/x.bin"));
    CHECK(IsSafePackageEntryName("a/b/c.lua"));
    CHECK(IsSafePackageEntryName(std::string(4096, 'a')));

    const std::string tooLong(4097, 'a');
    for (const std::string_view rejected : { std::string_view(), std::string_view(".."),
             std::string_view("../x"), std::string_view("a/../b"), std::string_view("/x"),
             std::string_view("\\x"), std::string_view("x/"), std::string_view("x\\"),
             std::string_view("C:/x"), std::string_view("a//b"), std::string_view(tooLong) })
    {
        INFO("entry: ", std::string(rejected.substr(0, 32)));
        CHECK_FALSE(IsSafePackageEntryName(rejected));
    }
}

TEST_CASE("ToLowerAscii lowercases ASCII and preserves everything else")
{
    CHECK(ToLowerAscii("") == "");
    CHECK(ToLowerAscii("ABC") == "abc");
    CHECK(ToLowerAscii("Assets/Meshes/Box.BIN") == "assets/meshes/box.bin");

    // Archive lookups follow Windows path semantics, so only ASCII case matters and
    // multi-byte sequences have to survive untouched.
    const std::string unicodeName = "Data/Ünïcode-123.bin";
    CHECK(ToLowerAscii(unicodeName).size() == unicodeName.size());
    CHECK(ToLowerAscii(unicodeName).substr(0, 5) == "data/");
}

TEST_CASE("GetPackageEntryDirectory splits entry names")
{
    CHECK(GetPackageEntryDirectory("x.bin").empty());
    CHECK(GetPackageEntryDirectory("Data/x.bin") == "Data");
    CHECK(GetPackageEntryDirectory("a/b/c") == "a/b");
    CHECK(GetPackageEntryDirectory("Assets/Meshes/Shared Meshes/Box Mesh.bin")
        == "Assets/Meshes/Shared Meshes");
}

TEST_CASE("ComputePackageTocHash covers every byte except the hash field")
{
    PackageTocHeader header {};
    header.EntryCount = 7;
    header.BlockCount = 9;
    header.NameTableSize = 128;
    header.TocContentHash = 0x1122334455667788ull;

    const uint64_t baseline = ComputePackageTocHash(&header, sizeof(header));
    CHECK(baseline != 0);
    CHECK(ComputePackageTocHash(&header, sizeof(header)) == baseline);

    // The hash field itself is excluded, so changing it alone cannot change the result.
    header.TocContentHash = 0;
    CHECK(ComputePackageTocHash(&header, sizeof(header)) == baseline);
    header.TocContentHash = 0x1122334455667788ull;

    // Every other byte is covered.
    header.EntryCount = 8;
    CHECK(ComputePackageTocHash(&header, sizeof(header)) != baseline);
    header.EntryCount = 7;

    header.NameTableSize = 129;
    CHECK(ComputePackageTocHash(&header, sizeof(header)) != baseline);
    header.NameTableSize = 128;

    // Buffers too small to hold the hash field cannot be hashed at all.
    CHECK(ComputePackageTocHash(nullptr, 0) == 0);
    CHECK(ComputePackageTocHash(&header, 8) == 0);
    CHECK(ComputePackageTocHash(&header, offsetof(PackageTocHeader, TocContentHash) + 4) == 0);
}

TEST_CASE("ParsePackageBuildInfo reads the documented keys and defaults")
{
    const std::string complete =
        "ProductName: RoundTrip\n"
        "StartScene: Assets/Scenes/Default.hscene\n"
        "WindowWidth: 1280\n"
        "WindowHeight: 720\n"
        "VSync: false\n";

    PackageBuildInfo buildInfo;
    REQUIRE(ParsePackageBuildInfo(complete, buildInfo));
    CHECK(buildInfo.IsValid());
    CHECK(buildInfo.ProductName == "RoundTrip");
    CHECK(buildInfo.StartScene == std::filesystem::path("Assets/Scenes/Default.hscene"));
    CHECK(buildInfo.WindowWidth == 1280u);
    CHECK(buildInfo.WindowHeight == 720u);
    CHECK(buildInfo.VSync == false);

    // Missing keys fall back to the documented defaults.
    const std::string minimal = "ProductName: Minimal\nStartScene: Assets/Start.hscene\n";
    REQUIRE(ParsePackageBuildInfo(minimal, buildInfo));
    CHECK(buildInfo.WindowWidth == 1600u);
    CHECK(buildInfo.WindowHeight == 900u);
    CHECK(buildInfo.VSync == true);

    // A package without a product name and a start scene is not usable.
    CHECK_FALSE(ParsePackageBuildInfo("WindowWidth: 640\n", buildInfo));
    CHECK_FALSE(buildInfo.IsValid());

    // Broken or empty documents fail and leave the defaults behind.
    buildInfo = {};
    CHECK_FALSE(ParsePackageBuildInfo("", buildInfo));
    CHECK_FALSE(buildInfo.IsValid());
    CHECK(buildInfo.ProductName.empty());
    CHECK(buildInfo.WindowWidth == 1600u);

    CHECK_FALSE(ParsePackageBuildInfo("[unclosed", buildInfo));
    CHECK_FALSE(buildInfo.IsValid());
}

TEST_SUITE_END();
