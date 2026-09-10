#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <string>

namespace HachimiEngine
{
    class Project;

    // YAML persistence for .hproj project descriptor files.
    class ProjectSerializer
    {
    public:
        explicit ProjectSerializer(const Ref<Project>& project);

        void Serialize(const std::string& filepath);
        // Writes a project descriptor intended for a packaged game build. All
        // paths are stored relative to the file location so the package can be
        // extracted and run from any directory.
        void SerializeForBuild(const std::string& filepath);
        std::string SerializeForBuildToString();
        bool Deserialize(const std::string& filepath);

    private:
        Ref<Project> m_Project;
    };
}
