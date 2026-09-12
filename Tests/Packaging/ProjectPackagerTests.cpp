// ProjectPackager: the export pipeline the editor's Build Settings drive.
//
// The suite builds a small project directory by hand and points a Project at it, which is
// what the editor does after loading a .hproj. Project::CreateNew() is not used here: it
// builds showcase meshes through OpenGL, which a headless test cannot do.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Packaging/GameBuildSettings.h"
#include "Packaging/PackageFormat.h"
#include "Packaging/PackageReader.h"
#include "Packaging/ProjectPackager.h"
#include "Project/Project.h"
#include "Serialization/ProjectSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    struct ExportFixture
    {
        std::filesystem::path ProjectDirectory;
        std::filesystem::path PlayerExecutable;
        Ref<Project> Project;
    };

    // A project directory that looks like one the editor produced: a start scene, a script,
    // one texture, and a stand-in for the built Hachimi-Player.exe.
    ExportFixture MakeExportFixture(const std::string& directoryName)
    {
        ExportFixture fixture;
        fixture.ProjectDirectory = Workspace().PrepareDirectory(directoryName);

        const std::filesystem::path assets = fixture.ProjectDirectory / "Assets";
        REQUIRE(FileSystem::WriteTextFile(assets / "Scenes" / "Default.hscene",
            "Scene: Default\nEntities: []\n"));
        REQUIRE(FileSystem::WriteTextFile(assets / "Scripts" / "Rotator.lua", "return {}\n"));
        REQUIRE(WriteBytes(assets / "Textures" / "Grid.png", MakeRandomBytes(512, 21)));

        fixture.PlayerExecutable = WritePlayerStandIn(fixture.ProjectDirectory / "tools");

        fixture.Project = CreateRef<Project>();
        fixture.Project->SetName("ExportProject");
        fixture.Project->SetProjectDirectory(fixture.ProjectDirectory);
        fixture.Project->SetAssetsDirectory(assets);
        fixture.Project->SetStartScenePath(assets / "Scenes" / "Default.hscene");
        fixture.Project->SetProjectFilePath(fixture.ProjectDirectory / "ExportProject.hproj");

        PlatformBuildSettings& windows = fixture.Project->GetBuildSettings().GetOrCreateWindowsSettings();
        windows.ProductName = "ExportSmoke";
        windows.StartScene = "Assets/Scenes/Default.hscene";
        windows.WindowWidth = 1280;
        windows.WindowHeight = 720;
        windows.VSync = true;

        return fixture;
    }
}

TEST_SUITE_BEGIN("Packaging");

TEST_CASE("ProjectPackager writes the game folder, the player copy and a readable package")
{
    ExportFixture fixture = MakeExportFixture("export_success");

    const GameExportResult result = ProjectPackager::Export(fixture.Project, fixture.PlayerExecutable);
    INFO("message: ", result.Message);
    REQUIRE(result.Success);

    const std::filesystem::path expectedOutput = fixture.ProjectDirectory / "Build" / "Windows_x64";
    CHECK(result.OutputDirectory == expectedOutput);
    CHECK(result.ExecutablePath == expectedOutput / "ExportSmoke.exe");
    CHECK(result.PackagePath == expectedOutput / "Data.hpak");
    CHECK(FileSystem::Exists(result.ExecutablePath));
    CHECK(FileSystem::GetFileSize(result.ExecutablePath) == FileSystem::GetFileSize(fixture.PlayerExecutable));

    PackageReader reader;
    REQUIRE(reader.Open(result.PackagePath));

    for (const std::string& entry : { std::string("BuildInfo.yaml"), std::string("Project.hproj"),
             std::string("Assets/Scenes/Default.hscene"), std::string("Assets/Scripts/Rotator.lua"),
             std::string("Assets/Textures/Grid.png") })
    {
        INFO("entry: ", entry);
        CHECK(reader.HasEntry(entry));
    }

    // The engine shaders travel with every exported game.
    CHECK_FALSE(reader.EnumerateEntries("Shaders", false).empty());

    size_t buildInfoIndex = 0;
    REQUIRE(reader.FindEntry("BuildInfo.yaml", buildInfoIndex));
    std::vector<uint8_t> buildInfoBytes;
    REQUIRE(reader.ReadEntry(buildInfoIndex, buildInfoBytes));

    PackageBuildInfo buildInfo;
    REQUIRE(ParsePackageBuildInfo(std::string(buildInfoBytes.begin(), buildInfoBytes.end()), buildInfo));
    CHECK(buildInfo.ProductName == "ExportSmoke");
    CHECK(buildInfo.StartScene == std::filesystem::path("Assets/Scenes/Default.hscene"));
    CHECK(buildInfo.WindowWidth == 1280u);
    CHECK(buildInfo.WindowHeight == 720u);
    CHECK(buildInfo.VSync == true);

    // The packaged project descriptor is relative to the package, so an exported game can
    // be moved anywhere and still find its assets.
    size_t projectIndex = 0;
    REQUIRE(reader.FindEntry("Project.hproj", projectIndex));
    std::vector<uint8_t> projectBytes;
    REQUIRE(reader.ReadEntry(projectIndex, projectBytes));

    const std::filesystem::path unpackedDirectory = Workspace().PrepareDirectory("export_unpacked");
    const std::filesystem::path projectFile = unpackedDirectory / "Project.hproj";
    REQUIRE(WriteBytes(projectFile, projectBytes));

    Ref<Project> reloaded = CreateRef<Project>();
    ProjectSerializer serializer(reloaded);
    REQUIRE(serializer.Deserialize(projectFile.string()));
    CHECK(reloaded->GetName() == "ExportProject");

    // The descriptor stores "ProjectDirectory: .", so the reloaded project points at the
    // directory the descriptor was unpacked into. equivalent() compares that identity,
    // because MSVC's lexically_normal() spells a path ending in "." with a trailing
    // separator.
    std::error_code errorCode;
    CHECK(std::filesystem::equivalent(reloaded->GetProjectDirectory(), unpackedDirectory, errorCode));
    CHECK_FALSE(errorCode);

    CHECK(reloaded->GetAssetsDirectory() == unpackedDirectory / "Assets");
    CHECK(reloaded->GetStartScenePath() == unpackedDirectory / std::filesystem::path("Assets/Scenes/Default.hscene"));
}

TEST_CASE("ProjectPackager reports validation failures instead of writing")
{
    ExportFixture fixture = MakeExportFixture("export_failures");
    const std::filesystem::path outputDirectory = fixture.ProjectDirectory / "Build";

    const auto expectFailure = [&](const Ref<Project>& project, const std::filesystem::path& player,
        const std::string& what)
    {
        INFO("case: ", what);
        const GameExportResult result = ProjectPackager::Export(project, player);
        CHECK_FALSE(result.Success);
        CHECK_FALSE(result.Message.empty());
        CHECK_FALSE(FileSystem::Exists(outputDirectory));
    };

    PlatformBuildSettings& windows = fixture.Project->GetBuildSettings().GetOrCreateWindowsSettings();
    const PlatformBuildSettings validSettings = windows;

    expectFailure(nullptr, fixture.PlayerExecutable, "no project");

    Ref<Project> withoutSettings = CreateRef<Project>();
    withoutSettings->SetName("NoSettings");
    withoutSettings->SetProjectDirectory(fixture.ProjectDirectory);
    withoutSettings->SetAssetsDirectory(fixture.ProjectDirectory / "Assets");
    expectFailure(withoutSettings, fixture.PlayerExecutable, "no Windows build settings");

    windows.ProductName = "";
    expectFailure(fixture.Project, fixture.PlayerExecutable, "empty product name");
    windows = validSettings;

    for (const char* productName : { "bad/name", " bad", "bad.", "bad:name" })
    {
        windows.ProductName = productName;
        expectFailure(fixture.Project, fixture.PlayerExecutable, std::string("invalid product name '") + productName + "'");
    }
    windows = validSettings;

    windows.StartScene = "";
    expectFailure(fixture.Project, fixture.PlayerExecutable, "empty start scene");
    windows = validSettings;

    windows.StartScene = "Assets/Scenes/Missing.hscene";
    expectFailure(fixture.Project, fixture.PlayerExecutable, "start scene that does not exist");
    windows = validSettings;

    expectFailure(fixture.Project, fixture.ProjectDirectory / "tools" / "NoPlayer.exe",
        "player executable that does not exist");

    // Nothing above was allowed to leave an export behind.
    CHECK_FALSE(FileSystem::Exists(outputDirectory));
}

TEST_SUITE_END();
