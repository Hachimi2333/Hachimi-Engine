#include "Serialization/ProjectSerializer.h"

#include "Core/Log.h"
#include "Project/Project.h"
#include "Utils/FileSystem.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace HachimiEngine
{
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
        out << YAML::EndMap;

        std::ofstream file(filepath);
        file << out.c_str();
    }

    bool ProjectSerializer::Deserialize(const std::string& filepath)
    {
        const YAML::Node data = YAML::LoadFile(filepath);
        if (!data || !data["Project"])
        {
            HE_CORE_ERROR("Failed to load project file: {}", filepath);
            return false;
        }

        m_Project->SetName(data["Project"].as<std::string>());
        m_Project->SetProjectDirectory(data["ProjectDirectory"].as<std::string>());
        m_Project->SetAssetsDirectory(data["AssetsDirectory"].as<std::string>());
        m_Project->SetStartScenePath(data["StartScene"].as<std::string>());
        m_Project->SetProjectFilePath(filepath);

        return true;
    }
}
