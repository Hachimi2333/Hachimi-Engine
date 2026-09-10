#include "Serialization/ProjectSerializer.h"

#include "Core/Log.h"
#include "Project/Project.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        std::filesystem::path ResolveProjectPath(
            const YAML::Node& node,
            const std::filesystem::path& baseDirectory,
            const std::filesystem::path& fallback)
        {
            if (!node)
            {
                return fallback;
            }

            std::filesystem::path path(node.as<std::string>());
            if (path.empty())
            {
                return fallback;
            }

            if (!path.is_absolute())
            {
                path = baseDirectory / path;
            }
            return path.lexically_normal();
        }

        std::filesystem::path GetRelativeScenePath(const Project& project)
        {
            std::error_code errorCode;
            std::filesystem::path relativePath =
                std::filesystem::relative(project.GetStartScenePath(), project.GetProjectDirectory(), errorCode);
            if (errorCode)
            {
                relativePath = "Assets" / std::filesystem::path("Scenes") / project.GetStartScenePath().filename();
            }
            return relativePath.lexically_normal();
        }

        PlatformBuildSettings ReadPlatformSettings(
            const YAML::Node& node,
            const std::string& productNameFallback,
            const std::filesystem::path& startSceneFallback)
        {
            PlatformBuildSettings settings;
            settings.ProductName = node["ProductName"].as<std::string>(productNameFallback);
            settings.StartScene = node["StartScene"]
                ? std::filesystem::path(node["StartScene"].as<std::string>())
                : startSceneFallback;
            settings.WindowWidth = node["WindowWidth"].as<uint32_t>(settings.WindowWidth);
            settings.WindowHeight = node["WindowHeight"].as<uint32_t>(settings.WindowHeight);
            settings.VSync = node["VSync"].as<bool>(settings.VSync);
            return settings;
        }

        void EmitPlatformSettings(YAML::Emitter& out, const PlatformBuildSettings& settings)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "ProductName" << YAML::Value << settings.ProductName;
            out << YAML::Key << "StartScene" << YAML::Value << settings.StartScene.generic_string();
            out << YAML::Key << "WindowWidth" << YAML::Value << settings.WindowWidth;
            out << YAML::Key << "WindowHeight" << YAML::Value << settings.WindowHeight;
            out << YAML::Key << "VSync" << YAML::Value << settings.VSync;
            out << YAML::EndMap;
        }
    }

    ProjectSerializer::ProjectSerializer(const Ref<Project>& project)
        : m_Project(project)
    {
    }

    void ProjectSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value << m_Project->GetName();
        out << YAML::Key << "ProjectDirectory" << YAML::Value << m_Project->GetProjectDirectory().string();
        out << YAML::Key << "AssetsDirectory" << YAML::Value << m_Project->GetAssetsDirectory().string();
        out << YAML::Key << "StartScene" << YAML::Value << m_Project->GetStartScenePath().string();

        out << YAML::Key << "BuildSettings" << YAML::Value << YAML::BeginMap;
        for (const auto& [platformName, settings] : m_Project->GetBuildSettings().Platforms)
        {
            out << YAML::Key << platformName << YAML::Value;
            EmitPlatformSettings(out, settings);
        }
        out << YAML::EndMap;

        out << YAML::EndMap;

        std::ofstream file(filepath);
        file << out.c_str();
    }

    void ProjectSerializer::SerializeForBuild(const std::string& filepath)
    {
        const std::string serialized = SerializeForBuildToString();
        std::ofstream file(filepath);
        file << serialized;
    }

    std::string ProjectSerializer::SerializeForBuildToString()
    {
        const PlatformBuildSettings* settings = m_Project->GetBuildSettings().GetWindowsSettings();
        const std::filesystem::path startScene = settings != nullptr && !settings->StartScene.empty()
            ? settings->StartScene
            : GetRelativeScenePath(*m_Project);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value << m_Project->GetName();
        out << YAML::Key << "ProjectDirectory" << YAML::Value << ".";
        out << YAML::Key << "AssetsDirectory" << YAML::Value << "Assets";
        out << YAML::Key << "StartScene" << YAML::Value << startScene.generic_string();
        out << YAML::EndMap;
        return out.c_str();
    }

    bool ProjectSerializer::Deserialize(const std::string& filepath)
    {
        // Read through the virtual file system: a packaged Project.hproj lives
        // inside the game package, not on disk.
        std::string projectText;
        if (!VirtualFileSystem::ReadTextFile(filepath, projectText))
        {
            HE_CORE_ERROR("Failed to read project file: {}", filepath);
            return false;
        }

        YAML::Node data;
        try
        {
            data = YAML::Load(projectText);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Failed to parse project file '{}': {}", filepath, exception.what());
            return false;
        }

        if (!data || !data["Project"])
        {
            HE_CORE_ERROR("Failed to load project file: {}", filepath);
            return false;
        }

        const std::filesystem::path projectFilePath(filepath);
        const std::filesystem::path projectDirectory = projectFilePath.parent_path();

        m_Project->SetName(data["Project"].as<std::string>());
        m_Project->SetProjectFilePath(projectFilePath);

        const std::filesystem::path resolvedProjectDirectory =
            ResolveProjectPath(data["ProjectDirectory"], projectDirectory, projectDirectory);
        m_Project->SetProjectDirectory(resolvedProjectDirectory);

        const std::filesystem::path defaultAssetsDirectory = resolvedProjectDirectory / "Assets";
        m_Project->SetAssetsDirectory(ResolveProjectPath(data["AssetsDirectory"], resolvedProjectDirectory, defaultAssetsDirectory));

        const std::filesystem::path defaultStartScene = defaultAssetsDirectory / "Scenes" / "Default.hscene";
        m_Project->SetStartScenePath(ResolveProjectPath(data["StartScene"], resolvedProjectDirectory, defaultStartScene));

        GameBuildSettings& buildSettings = m_Project->GetBuildSettings();
        buildSettings.Platforms.clear();

        if (const YAML::Node buildSettingsNode = data["BuildSettings"])
        {
            for (const auto& platformEntry : buildSettingsNode)
            {
                const std::string platformName = platformEntry.first.as<std::string>();
                PlatformBuildSettings settings = ReadPlatformSettings(
                    platformEntry.second,
                    m_Project->GetName(),
                    GetRelativeScenePath(*m_Project));
                buildSettings.Platforms[platformName] = std::move(settings);
            }
        }

        PlatformBuildSettings& windowsSettings = buildSettings.GetOrCreateWindowsSettings();
        if (windowsSettings.ProductName.empty())
        {
            windowsSettings.ProductName = m_Project->GetName();
        }
        if (windowsSettings.StartScene.empty())
        {
            windowsSettings.StartScene = GetRelativeScenePath(*m_Project);
        }

        return true;
    }
}
