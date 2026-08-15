#include "Asset/TextureImporter.h"

#include "Utils/FileSystem.h"

namespace HachimiEngine
{
    Ref<Texture2D> TextureImporter::LoadTexture(const std::filesystem::path& path)
    {
        return Texture2D::Create(path.string());
    }

    bool TextureImporter::ImportTexture(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
    {
        return FileSystem::CopyFile(sourcePath, destinationPath);
    }
}
