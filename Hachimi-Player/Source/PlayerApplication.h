#pragma once

#include "Core/Application.h"
#include "Packaging/PackageFormat.h"

#include <filesystem>

namespace HachimiEngine
{
    // Standalone game process created by the export pipeline.
    class PlayerApplication final : public Application
    {
    public:
        PlayerApplication(PackageBuildInfo buildInfo, std::filesystem::path contentRoot);
        ~PlayerApplication() override = default;
    };
}
