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
        bool Deserialize(const std::string& filepath);

    private:
        Ref<Project> m_Project;
    };
}
