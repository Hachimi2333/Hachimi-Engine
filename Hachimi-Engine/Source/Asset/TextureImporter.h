#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Texture.h"

#include <filesystem>

namespace HachimiEngine
{
    // Loads image files into Texture2D resources and copies them into project Assets.
    class TextureImporter
    {
    public:
        static Ref<Texture2D> LoadTexture(const std::filesystem::path& path);
        static bool ImportTexture(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath);
    };
}
