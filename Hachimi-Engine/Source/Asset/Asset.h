#pragma once

#include "Core/Base.h"
#include "Core/UUID.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    enum class AssetType
    {
        None = 0,
        Texture = 1,
        Scene = 2,
        Project = 3
    };

    struct Asset
    {
        UUID ID;
        AssetType Type = AssetType::None;
        std::filesystem::path Path;
        std::string Name;
    };
}
