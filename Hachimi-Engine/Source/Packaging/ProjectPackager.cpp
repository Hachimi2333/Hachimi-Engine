#include "Packaging/ProjectPackager.h"

#include "Core/Log.h"
#include "Packaging/PackageFormat.h"
#include "Project/Project.h"
#include "Serialization/ProjectSerializer.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace HachimiEngine
{
    namespace
    {
        constexpr std::string_view InvalidProductNameCharacters = "<>:\"/\\|?*";

        bool IsValidProductName(const std::string& productName)
        {
            if (productName.empty())
            {
                return false;
            }

            if (productName.front() == ' ' || productName.back() == ' ' || productName.back() == '.')
            {
                return false;
            }

            return productName.find_first_of(InvalidProductNameCharacters) == std::string::npos;
        }

        std::string MakeBuildInfoYaml(const PlatformBuildSettings& settings)
        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out << YAML::Key << "ProductName" << YAML::Value << settings.ProductName;
            out << YAML::Key << "StartScene" << YAML::Value << settings.StartScene.generic_string();
            out << YAML::Key << "WindowWidth" << YAML::Value << settings.WindowWidth;
            out << YAML::Key << "WindowHeight" << YAML::Value << settings.WindowHeight;
            out << YAML::Key << "VSync" << YAML::Value << settings.VSync;
            out << YAML::EndMap;
            return out.c_str();
        }

        bool AddDirectoryFiles(
            PackageWriter& writer,
            const std::filesystem::path& directory,
            const std::filesystem::path& archiveRoot)
        {
            if (!FileSystem::IsDirectory(directory))
            {
                HE_CORE_ERROR("Cannot package missing directory: {}", directory.string());
                return false;
            }

            for (const std::filesystem::path& filePath : FileSystem::GetFilesRecursive(directory))
            {
                std::error_code errorCode;
                const std::filesystem::path relativePath = std::filesystem::relative(filePath, directory, errorCode);
                if (errorCode)
                {
                    HE_CORE_ERROR("Failed to make package path for {}", filePath.string());
                    return false;
                }

                if (!writer.AddFile(archiveRoot / relativePath, filePath))
                {
                    return false;
                }
            }
            return true;
        }

        bool AddEngineShaderFiles(PackageWriter& writer)
        {
            const std::filesystem::path shaderDirectory = PlatformUtils::GetExecutableDirectory() / "Shaders";
            bool addedAnyShader = false;

            for (const std::filesystem::path& shaderPath : FileSystem::GetFiles(shaderDirectory))
            {
                if (FileSystem::GetExtension(shaderPath) != ".glsl")
                {
                    continue;
                }

                if (!writer.AddFile("Shaders" / std::filesystem::path(FileSystem::GetFileName(shaderPath)), shaderPath))
                {
                    return false;
                }
                addedAnyShader = true;
            }

            if (!addedAnyShader)
            {
                HE_CORE_ERROR("No engine shaders found in {}", shaderDirectory.string());
                return false;
            }
            return true;
        }

        void AddOptionalEngineFonts(PackageWriter& writer)
        {
            const std::filesystem::path fontDirectory = PlatformUtils::GetExecutableDirectory() / "Assets" / "Fonts";
            if (!FileSystem::IsDirectory(fontDirectory))
            {
                HE_CORE_WARN("Engine fonts not found at {}; the exported game will use the default ImGui font", fontDirectory.string());
                return;
            }

            for (const std::filesystem::path& fontPath : FileSystem::GetFiles(fontDirectory))
            {
                if (!writer.AddFile("Assets" / std::filesystem::path("Fonts") / FileSystem::GetFileName(fontPath), fontPath))
                {
                    HE_CORE_WARN("Failed to package engine font {}", fontPath.string());
                }
            }
        }
    }

    std::filesystem::path ProjectPackager::GetWindowsOutputDirectory(const Project& project)
    {
        return project.GetProjectDirectory() / "Build" / "Windows_x64";
    }

    std::filesystem::path ProjectPackager::GetDataPackagePath(const Project& project)
    {
        return GetWindowsOutputDirectory(project) / "Data.hpak";
    }

    GameExportResult ProjectPackager::Export(const Ref<Project>& project, const std::filesystem::path& playerExecutablePath)
    {
        GameExportResult result;

        if (project == nullptr)
        {
            result.Message = "Cannot export: no active project";
            return result;
        }

        const PlatformBuildSettings* settings = project->GetBuildSettings().GetWindowsSettings();
        if (settings == nullptr)
        {
            result.Message = "Cannot export: Windows build settings are missing";
            return result;
        }

        if (!IsValidProductName(settings->ProductName))
        {
            result.Message = "Cannot export: Product Name is empty or contains invalid characters";
            return result;
        }

        if (settings->StartScene.empty())
        {
            result.Message = "Cannot export: no Start Scene selected";
            return result;
        }

        const std::filesystem::path startScenePath = project->GetProjectDirectory() / settings->StartScene;
        if (!FileSystem::Exists(startScenePath))
        {
            result.Message = "Cannot export: start scene does not exist: " + startScenePath.string();
            return result;
        }

        if (!FileSystem::Exists(playerExecutablePath))
        {
            result.Message = "Cannot export: Hachimi-Player.exe was not found at " + playerExecutablePath.string()
                + ". Build the Hachimi-Player project first.";
            return result;
        }

        result.OutputDirectory = GetWindowsOutputDirectory(*project);
        if (FileSystem::Exists(result.OutputDirectory) && !FileSystem::RemoveAll(result.OutputDirectory))
        {
            result.Message = "Cannot export: failed to clean output directory " + result.OutputDirectory.string();
            return result;
        }

        if (!FileSystem::CreateDirectories(result.OutputDirectory))
        {
            result.Message = "Cannot export: failed to create output directory " + result.OutputDirectory.string();
            return result;
        }

        result.ExecutablePath = result.OutputDirectory / (settings->ProductName + ".exe");
        if (!FileSystem::CopyFile(playerExecutablePath, result.ExecutablePath))
        {
            result.Message = "Cannot export: failed to copy player executable to " + result.ExecutablePath.string();
            return result;
        }

        result.PackagePath = result.OutputDirectory / "Data.hpak";
        PackageWriter writer;
        if (!writer.Open(result.PackagePath))
        {
            result.Message = "Cannot export: failed to create package " + result.PackagePath.string();
            return result;
        }

        const std::string buildInfoYaml = MakeBuildInfoYaml(*settings);
        if (!writer.AddMemory("BuildInfo.yaml", buildInfoYaml.data(), buildInfoYaml.size()))
        {
            result.Message = "Cannot export: failed to write BuildInfo.yaml";
            return result;
        }

        ProjectSerializer projectSerializer(project);
        const std::string buildProjectYaml = projectSerializer.SerializeForBuildToString();
        if (!writer.AddMemory("Project.hproj", buildProjectYaml.data(), buildProjectYaml.size()))
        {
            result.Message = "Cannot export: failed to write Project.hproj";
            return result;
        }

        if (!AddDirectoryFiles(writer, project->GetAssetsDirectory(), "Assets"))
        {
            result.Message = "Cannot export: failed to package project assets";
            return result;
        }

        if (!AddEngineShaderFiles(writer))
        {
            result.Message = "Cannot export: failed to package engine shaders";
            return result;
        }

        AddOptionalEngineFonts(writer);

        if (!writer.Finalize(result.Report))
        {
            result.Message = "Cannot export: failed to finalize package";
            return result;
        }

        result.Success = true;
        result.Message = "Export succeeded: " + result.OutputDirectory.string();
        HE_CORE_INFO("Exported '{}': {} entries, {:.1f}% of {} KiB after zstd (dictionary {} bytes, {:.2f}s)",
            settings->ProductName,
            result.Report.EntryCount,
            result.Report.CompressionRatio() * 100.0,
            result.Report.UncompressedBytes / 1024,
            result.Report.DictionarySize,
            result.Report.Seconds);
        return result;
    }
}
