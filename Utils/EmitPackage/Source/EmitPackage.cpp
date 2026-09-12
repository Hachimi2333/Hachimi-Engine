// EmitPackage: writes a package laid out exactly like ProjectPackager produces one, so
// the Player's headless modes (--verify, --list, --stats) can be exercised against a real
// artifact without launching the editor.
//
//   EmitPackage.exe <output.hpak>
//
// The engine shaders and the UI font are taken from the directory of this executable, so
// run it from Build/<preset>/bin/ after a build.

#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Packaging/PackageFormat.h"
#include "Packaging/PackageWriter.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    void PrintUsage()
    {
        std::printf("Usage: EmitPackage.exe <output.hpak>\n"
                    "\n"
                    "Writes a smoke-test game package: BuildInfo.yaml, Project.hproj, a scene,\n"
                    "a Lua script, synthetic texture bytes and the engine's shaders and fonts.\n"
                    "Feed the result to Hachimi-Player.exe <package> --verify --list --stats.\n");
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
}

int main(int argc, char** argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        PrintUsage();
        return 0;
    }

    if (argc != 2)
    {
        PrintUsage();
        return 1;
    }

    const std::filesystem::path packagePath = argv[1];

    Log::Init();
    JobSystem::Init();

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

    int result = 0;

    FileSystem::CreateDirectories(packagePath.parent_path());

    PackageWriter writer;
    if (!writer.Open(packagePath))
    {
        std::printf("Failed to open '%s' for writing\n", packagePath.string().c_str());
        result = 1;
    }
    else
    {
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
            result = 1;
        }
        else
        {
            std::printf("Wrote %s\n  entries: %u (%u stored, %u zstd), blocks: %u, dictionary: %u\n"
                        "  raw: %llu bytes, packed: %llu bytes (%.1f%%), runtime files from %s: %zu\n",
                packagePath.string().c_str(),
                report.EntryCount, report.StoredEntryCount, report.ZstdEntryCount, report.BlockCount,
                report.DictionarySize,
                static_cast<unsigned long long>(report.UncompressedBytes),
                static_cast<unsigned long long>(report.CompressedBytes),
                report.CompressionRatio() * 100.0,
                exeDirectory.string().c_str(),
                runtimeFileCount);
        }
    }

    JobSystem::Shutdown();
    Log::Shutdown();
    return result;
}
