#include "PlayerApplication.h"

#include "Core/EntryPoint.h"
#include "Core/Log.h"
#include "Packaging/PackageFormat.h"
#include "Packaging/PackageReader.h"
#include "PlayerLayer.h"
#include "Utils/PlatformUtils.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace HachimiEngine
{
    PlayerApplication::PlayerApplication(PackageBuildInfo buildInfo, std::filesystem::path contentRoot)
        : Application(WindowProps(
            buildInfo.ProductName.empty() ? "Hachimi-Player" : buildInfo.ProductName,
            std::max(buildInfo.WindowWidth, 320u),
            std::max(buildInfo.WindowHeight, 240u)))
    {
        m_Window->SetVSync(buildInfo.VSync);
        HE_CLIENT_INFO("Hachimi-Player started");
        PushLayer(CreateRef<PlayerLayer>(std::move(buildInfo), std::move(contentRoot)));
    }

    namespace
    {
        constexpr int ExitSuccess = 0;
        constexpr int ExitFailure = 1;

        struct CommandLine
        {
            std::filesystem::path PackagePath;
            std::filesystem::path ContentDirectory;
            bool VerifyOnly = false;
            bool ListOnly = false;
            bool ShowStats = false;
            bool Help = false;
        };

        void PrintUsage()
        {
            std::printf(
                "Hachimi-Player - runs a packaged Hachimi game\n"
                "\n"
                "Usage: Hachimi-Player [<package>] [options]\n"
                "\n"
                "Options:\n"
                "  --content=<dir>  Mount a directory of loose files over the package (development)\n"
                "  --verify         Verify every packaged entry against its content hash, then exit\n"
                "  --list           Print the package table of contents, then exit\n"
                "  --stats          Print package header information, then exit\n"
                "  --help           Show this message\n"
                "\n"
                "Without <package>, Data.hpak next to the executable is used.\n");
        }

        bool ParseCommandLine(int argc, char** argv, CommandLine& outCommandLine)
        {
            outCommandLine.PackagePath = PlatformUtils::GetExecutableDirectory() / "Data.hpak";

            for (int index = 1; index < argc; ++index)
            {
                const std::string argument(argv[index]);
                if (argument.empty())
                {
                    continue;
                }

                if (argument == "--verify")
                {
                    outCommandLine.VerifyOnly = true;
                }
                else if (argument == "--list")
                {
                    outCommandLine.ListOnly = true;
                }
                else if (argument == "--stats")
                {
                    outCommandLine.ShowStats = true;
                }
                else if (argument == "--help" || argument == "-h" || argument == "/?")
                {
                    outCommandLine.Help = true;
                }
                else if (argument.starts_with("--content="))
                {
                    outCommandLine.ContentDirectory = argument.substr(std::string("--content=").size());
                }
                else if (argument.starts_with("--"))
                {
                    std::printf("Unknown option: %s\n", argument.c_str());
                    return false;
                }
                else
                {
                    outCommandLine.PackagePath = argument;
                }
            }

            return true;
        }

        // Headless package inspection. Running these before any window exists is
        // what makes the whole pipeline verifiable without a GPU.
        int RunPackageTool(const CommandLine& commandLine)
        {
            PackageReader reader;
            if (!reader.Open(commandLine.PackagePath))
            {
                std::printf("Failed to open package: %s\n", commandLine.PackagePath.string().c_str());
                return ExitFailure;
            }

            const PackageFileHeader& header = reader.GetHeader();
            std::uint64_t totalUncompressed = 0;
            std::uint64_t totalCompressed = 0;

            if (commandLine.ShowStats || commandLine.ListOnly)
            {
                std::printf("Package:        %s\n", commandLine.PackagePath.string().c_str());
                std::printf("Format version: %u\n", header.FormatVersion);
                std::printf("Package id:     0x%016llX\n", static_cast<unsigned long long>(header.PackageId));
                std::printf("Block size:     %u\n", header.BlockSize);
                std::printf("Dictionary:     %u bytes\n", header.DictionarySize);
                std::printf("Memory mapped:  %s\n", reader.IsMemoryMapped() ? "yes" : "no");
                std::printf("Entries:        %zu\n", reader.GetEntryCount());
            }

            if (commandLine.ListOnly)
            {
                std::printf("\n%-52s %12s %12s %8s %6s\n", "Path", "Size", "Packed", "Method", "Blocks");
                for (size_t index = 0; index < reader.GetEntryCount(); ++index)
                {
                    const PackageEntryInfo& entry = reader.GetEntry(index);
                    totalUncompressed += entry.UncompressedSize;
                    totalCompressed += entry.CompressedSize;
                    std::printf("%-52s %12llu %12llu %8s %6u\n",
                        entry.VirtualPath.generic_string().c_str(),
                        static_cast<unsigned long long>(entry.UncompressedSize),
                        static_cast<unsigned long long>(entry.CompressedSize),
                        entry.Method == PackageCompressionMethod::Store ? "store" : "zstd",
                        entry.BlockCount);
                }
                std::printf("\nTotal: %llu bytes packed into %llu bytes\n",
                    static_cast<unsigned long long>(totalUncompressed),
                    static_cast<unsigned long long>(totalCompressed));
            }

            if (commandLine.VerifyOnly)
            {
                PackageVerificationReport report;
                const bool ok = reader.VerifyAll(report);
                std::printf("Verified %u/%u entries, %u failed\n",
                    report.VerifiedCount, report.EntryCount, report.FailedCount);
                for (const std::string& failure : report.Failures)
                {
                    std::printf("  FAILED: %s\n", failure.c_str());
                }
                return ok ? ExitSuccess : ExitFailure;
            }

            return ExitSuccess;
        }
    }

    Application* CreateApplication(int argc, char** argv)
    {
        CommandLine commandLine;
        if (!ParseCommandLine(argc, argv, commandLine) || commandLine.Help)
        {
            PrintUsage();
            // EntryPoint owns main(), so a headless decision ends the process here.
            std::exit(commandLine.Help ? ExitSuccess : ExitFailure);
        }

        if (commandLine.VerifyOnly || commandLine.ListOnly || commandLine.ShowStats)
        {
            const int exitCode = RunPackageTool(commandLine);
            std::exit(exitCode);
        }

        PackageBuildInfo buildInfo;
        buildInfo.ProductName = "Hachimi-Player";

        // Publish the package contents before the Application base constructor
        // creates the renderer, which loads engine shaders and the UI font.
        const std::filesystem::path contentRoot = commandLine.PackagePath.parent_path();
        if (!VirtualFileSystem::MountArchive(commandLine.PackagePath, contentRoot))
        {
            HE_CLIENT_ERROR("Could not mount game package '{}'", commandLine.PackagePath.string());
            return new PlayerApplication(std::move(buildInfo), {});
        }

        if (!commandLine.ContentDirectory.empty())
        {
            // Higher priority so loose files shadow the package for iteration.
            VirtualFileSystem::MountDirectory(commandLine.ContentDirectory, contentRoot, 10);
        }

        std::string buildInfoText;
        if (VirtualFileSystem::ReadTextFile(contentRoot / "BuildInfo.yaml", buildInfoText)
            && ParsePackageBuildInfo(buildInfoText, buildInfo))
        {
            HE_CLIENT_INFO("Mounted game package '{}'", commandLine.PackagePath.string());
        }
        else
        {
            HE_CLIENT_WARN("Game package '{}' has no valid BuildInfo.yaml; using defaults",
                commandLine.PackagePath.string());
        }

        return new PlayerApplication(std::move(buildInfo), contentRoot);
    }
}
