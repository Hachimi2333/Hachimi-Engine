// ProjectSerializer: .hproj round trips and the package-relative build descriptor.
//
// ProjectSerializer only touches YAML, the file system and the virtual file system, so
// the whole suite runs without a window. Project objects are assembled with setters
// because Project::CreateNew() builds meshes through OpenGL.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Packaging/GameBuildSettings.h"
#include "Project/Project.h"
#include "Serialization/ProjectSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <filesystem>
#include <string>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    struct ProjectFixture
    {
        std::filesystem::path Directory;
        Ref<Project> Project;
    };

    ProjectFixture MakeProjectFixture(const std::string& directoryName)
    {
        ProjectFixture fixture;
        fixture.Directory = Workspace().PrepareDirectory(directoryName);

        fixture.Project = CreateRef<Project>();
        fixture.Project->SetName("RoundTrip");
        fixture.Project->SetProjectDirectory(fixture.Directory);
        fixture.Project->SetAssetsDirectory(fixture.Directory / "Assets");
        fixture.Project->SetStartScenePath(fixture.Directory / "Assets" / "Scenes" / "Default.hscene");
        fixture.Project->SetProjectFilePath(fixture.Directory / "RoundTrip.hproj");

        GameBuildSettings& buildSettings = fixture.Project->GetBuildSettings();

        PlatformBuildSettings& windows = buildSettings.GetOrCreateWindowsSettings();
        windows.ProductName = "RoundTripGame";
        windows.StartScene = "Assets/Scenes/Default.hscene";
        windows.WindowWidth = 1920;
        windows.WindowHeight = 1080;
        windows.VSync = false;

        // A second platform entry proves the writer keeps every platform it is given.
        PlatformBuildSettings& linux = buildSettings.Platforms[GameBuildSettings::LinuxPlatformName];
        linux.ProductName = "RoundTripGame";
        linux.StartScene = "Assets/Scenes/Default.hscene";
        linux.WindowWidth = 1280;
        linux.WindowHeight = 720;
        linux.VSync = true;

        return fixture;
    }
}

TEST_SUITE_BEGIN("Serialization");

TEST_CASE("a project descriptor round trips through YAML")
{
    ProjectFixture fixture = MakeProjectFixture("project_roundtrip");
    const std::filesystem::path file = fixture.Directory / "RoundTrip.hproj";

    ProjectSerializer writer(fixture.Project);
    writer.Serialize(file.string());
    REQUIRE(FileSystem::Exists(file));

    Ref<Project> reloaded = CreateRef<Project>();
    ProjectSerializer reader(reloaded);
    REQUIRE(reader.Deserialize(file.string()));

    CHECK(reloaded->GetName() == "RoundTrip");
    CHECK(reloaded->GetProjectDirectory() == fixture.Directory);
    CHECK(reloaded->GetAssetsDirectory() == fixture.Directory / "Assets");
    CHECK(reloaded->GetStartScenePath() == fixture.Project->GetStartScenePath());
    CHECK(reloaded->GetProjectFilePath() == file);

    const GameBuildSettings& settings = reloaded->GetBuildSettings();
    CHECK(settings.Platforms.size() == 2);

    const PlatformBuildSettings* windows = settings.GetWindowsSettings();
    REQUIRE(windows != nullptr);
    CHECK(windows->ProductName == "RoundTripGame");
    CHECK(windows->StartScene.generic_string() == "Assets/Scenes/Default.hscene");
    CHECK(windows->WindowWidth == 1920u);
    CHECK(windows->WindowHeight == 1080u);
    CHECK(windows->VSync == false);

    const auto linux = settings.Platforms.find(GameBuildSettings::LinuxPlatformName);
    REQUIRE(linux != settings.Platforms.end());
    CHECK(linux->second.ProductName == "RoundTripGame");
    CHECK(linux->second.WindowWidth == 1280u);
    CHECK(linux->second.WindowHeight == 720u);
    CHECK(linux->second.VSync == true);
}

TEST_CASE("the build descriptor stays relative to the packaged file")
{
    ProjectFixture fixture = MakeProjectFixture("project_build");

    const std::filesystem::path directory = fixture.Directory / "package";
    REQUIRE(FileSystem::CreateDirectories(directory));
    const std::filesystem::path file = directory / "Project.hproj";

    ProjectSerializer writer(fixture.Project);
    writer.SerializeForBuild(file.string());
    REQUIRE(FileSystem::Exists(file));

    Ref<Project> reloaded = CreateRef<Project>();
    ProjectSerializer reader(reloaded);
    REQUIRE(reader.Deserialize(file.string()));

    CHECK(reloaded->GetName() == "RoundTrip");

    // The packaged descriptor stores "ProjectDirectory: .", which the reader resolves to
    // the directory the file lives in. equivalent() compares that identity, because MSVC's
    // lexically_normal() spells a path ending in "." with a trailing separator.
    std::error_code errorCode;
    CHECK(std::filesystem::equivalent(reloaded->GetProjectDirectory(), directory, errorCode));
    CHECK_FALSE(errorCode);

    CHECK(reloaded->GetAssetsDirectory() == directory / "Assets");
    CHECK(reloaded->GetStartScenePath() == directory / std::filesystem::path("Assets/Scenes/Default.hscene"));

    // The build descriptor carries no platform settings, so the reader fills in a usable
    // Windows entry from the project itself.
    const PlatformBuildSettings* windows = reloaded->GetBuildSettings().GetWindowsSettings();
    REQUIRE(windows != nullptr);
    CHECK(windows->ProductName == "RoundTrip");
    CHECK(windows->StartScene.generic_string() == "Assets/Scenes/Default.hscene");
}

TEST_CASE("a project descriptor with missing keys falls back to defaults")
{
    const std::filesystem::path directory = Workspace().PrepareDirectory("project_defaults");
    const std::filesystem::path file = directory / "Minimal.hproj";
    REQUIRE(FileSystem::WriteTextFile(file, "Project: Minimal\n"));

    Ref<Project> project = CreateRef<Project>();
    ProjectSerializer reader(project);
    REQUIRE(reader.Deserialize(file.string()));

    CHECK(project->GetName() == "Minimal");
    CHECK(project->GetProjectDirectory() == directory);
    CHECK(project->GetAssetsDirectory() == directory / "Assets");
    CHECK(project->GetStartScenePath() == directory / "Assets" / "Scenes" / "Default.hscene");

    const PlatformBuildSettings* windows = project->GetBuildSettings().GetWindowsSettings();
    REQUIRE(windows != nullptr);
    CHECK(windows->ProductName == "Minimal");
    CHECK(windows->StartScene.generic_string() == "Assets/Scenes/Default.hscene");
    CHECK(windows->WindowWidth == 1600u);
    CHECK(windows->WindowHeight == 900u);
    CHECK(windows->VSync == true);
}

TEST_CASE("a missing or malformed project descriptor fails cleanly")
{
    const std::filesystem::path directory = Workspace().PrepareDirectory("project_broken");

    Ref<Project> project = CreateRef<Project>();
    project->SetName("Untouched");

    ProjectSerializer reader(project);

    CHECK_FALSE(reader.Deserialize((directory / "missing.hproj").string()));
    CHECK(project->GetName() == "Untouched");

    const std::filesystem::path empty = directory / "empty.hproj";
    REQUIRE(FileSystem::WriteTextFile(empty, ""));
    CHECK_FALSE(reader.Deserialize(empty.string()));
    CHECK(project->GetName() == "Untouched");

    const std::filesystem::path malformed = directory / "malformed.hproj";
    REQUIRE(FileSystem::WriteTextFile(malformed, "[unclosed\n"));
    CHECK_FALSE(reader.Deserialize(malformed.string()));
    CHECK(project->GetName() == "Untouched");

    const std::filesystem::path withoutProjectKey = directory / "no_project.hproj";
    REQUIRE(FileSystem::WriteTextFile(withoutProjectKey, "Something: else\n"));
    CHECK_FALSE(reader.Deserialize(withoutProjectKey.string()));
    CHECK(project->GetName() == "Untouched");
}

TEST_SUITE_END();
