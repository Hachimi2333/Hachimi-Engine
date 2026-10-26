#pragma once

#include "Core/Memory.h"
#include "Packaging/GameBuildSettings.h"

#include <string>

namespace HachimiEngine
{
    class Project;

    // Keeps unfinished UI edits out of the live project until a successful save.
    class ProjectSettingsDraft
    {
    public:
        PlatformBuildSettings Value;
        void Bind(const Project& project);
        bool Save(const Ref<Project>& project, std::string& error);
    };
}
