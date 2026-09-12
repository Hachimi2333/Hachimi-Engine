// FileSystem: the thin std::filesystem wrapper every other subsystem builds on.

#include <doctest/doctest.h>

#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE_BEGIN("Utils");

TEST_CASE("file writes, copies and removals round trip")
{
    const std::filesystem::path directory = Workspace().PrepareDirectory("filesystem");
    const std::vector<uint8_t> bytes = MakeRandomBytes(4096, 3);

    // WriteBinaryFile and WriteTextFile create the parent directories themselves.
    const std::filesystem::path binary = directory / "nested" / "payload.bin";
    REQUIRE(WriteBytes(binary, bytes));
    CHECK(FileSystem::Exists(binary));
    CHECK(FileSystem::GetFileSize(binary) == bytes.size());

    const std::filesystem::path text = directory / "notes" / "readme.txt";
    const std::string content = "Hachimi-Engine\n";
    REQUIRE(FileSystem::WriteTextFile(text, content));
    CHECK(FileSystem::GetFileSize(text) == content.size());

    std::string readBack;
    REQUIRE(VirtualFileSystem::ReadTextFile(text, readBack));
    CHECK(readBack == content);

    const std::filesystem::path copy = directory / "payload_copy.bin";
    REQUIRE(FileSystem::CopyFile(binary, copy));
    CHECK(FileSystem::GetFileSize(copy) == bytes.size());

    // Copying over an existing file replaces it.
    REQUIRE(FileSystem::CopyFile(text, copy));
    CHECK(FileSystem::GetFileSize(copy) == content.size());

    CHECK(FileSystem::RemoveAll(directory / "nested"));
    CHECK_FALSE(FileSystem::Exists(directory / "nested"));
    CHECK_FALSE(FileSystem::Exists(binary));

    // Removing something that is not there is not an error.
    CHECK(FileSystem::RemoveAll(directory / "nested"));
    CHECK(FileSystem::RemoveAll(directory / "does_not_exist"));
}

TEST_CASE("path helpers split names the way the engine expects")
{
    CHECK(FileSystem::GetFileName("Assets/Meshes/Box Mesh.bin") == "Box Mesh.bin");
    CHECK(FileSystem::GetFileNameWithoutExtension("Assets/Scripts/Rotator.lua") == "Rotator");
    CHECK(FileSystem::GetExtension("Assets/Scripts/Rotator.lua") == ".lua");
    CHECK(FileSystem::GetExtension("Assets/no_extension").empty());
    CHECK(FileSystem::GetParentPath("Assets/Meshes/Box Mesh.bin") == std::filesystem::path("Assets/Meshes"));
    CHECK(FileSystem::GetParentPath("x.bin").empty());
}

TEST_CASE("directory listings are complete and sorted")
{
    const std::filesystem::path directory = Workspace().PrepareDirectory("listing");

    REQUIRE(FileSystem::CreateDirectories(directory / "sub" / "deeper"));
    REQUIRE(FileSystem::WriteTextFile(directory / "a.txt", "a"));
    REQUIRE(FileSystem::WriteTextFile(directory / "b.txt", "b"));
    REQUIRE(FileSystem::WriteTextFile(directory / "sub" / "c.txt", "c"));
    REQUIRE(FileSystem::WriteTextFile(directory / "sub" / "deeper" / "d.txt", "d"));

    const std::vector<std::filesystem::path> files = FileSystem::GetFiles(directory);
    CHECK(files.size() == 2);
    CHECK(std::is_sorted(files.begin(), files.end()));

    const std::vector<std::filesystem::path> directories = FileSystem::GetDirectories(directory);
    REQUIRE(directories.size() == 1);
    CHECK(directories.front() == directory / "sub");

    const std::vector<std::filesystem::path> recursive = FileSystem::GetFilesRecursive(directory);
    CHECK(recursive.size() == 4);
    CHECK(std::is_sorted(recursive.begin(), recursive.end()));

    // A directory that is not there lists nothing instead of throwing.
    CHECK(FileSystem::GetFiles(directory / "missing").empty());
    CHECK(FileSystem::GetFilesRecursive(directory / "missing").empty());
    CHECK(FileSystem::GetDirectories(directory / "missing").empty());
}

TEST_CASE("missing paths report as missing instead of throwing")
{
    const std::filesystem::path directory = Workspace().PrepareDirectory("missing_paths");
    const std::filesystem::path missing = directory / "nope.bin";

    CHECK(FileSystem::IsDirectory(directory));
    CHECK_FALSE(FileSystem::Exists(missing));
    CHECK_FALSE(FileSystem::IsDirectory(missing));
    CHECK(FileSystem::GetFileSize(missing) == 0);
    CHECK(FileSystem::GetLastWriteTime(missing) == std::filesystem::file_time_type {});

    CHECK(FileSystem::CreateDirectories(directory / "a" / "b" / "c"));
    CHECK(FileSystem::IsDirectory(directory / "a" / "b" / "c"));
    CHECK(FileSystem::CreateDirectories(directory / "a" / "b" / "c"));
}

TEST_SUITE_END();
