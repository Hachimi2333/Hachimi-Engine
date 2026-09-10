#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Packaging/PackageWriter.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    class Project;

    struct GameExportResult
    {
        bool Success = false;
        std::string Message;
        std::filesystem::path OutputDirectory;
        std::filesystem::path ExecutablePath;
        std::filesystem::path PackagePath;
        PackageBuildReport Report;
    };

    // Creates the Windows_x64 export folder containing the game executable and
    // the packaged project data archive.
    class ProjectPackager
    {
    public:
        static std::filesystem::path GetWindowsOutputDirectory(const Project& project);
        static std::filesystem::path GetDataPackagePath(const Project& project);

        // playerExecutablePath is the built Hachimi-Player.exe, normally located
        // next to the running Hachimi-Editor.exe.
        static GameExportResult Export(const Ref<Project>& project, const std::filesystem::path& playerExecutablePath);
    };
}
