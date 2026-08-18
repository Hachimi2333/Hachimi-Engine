#include "Asset/AssetManager.h"

#include "Asset/TextureImporter.h"
#include "Core/Log.h"
#include "Utils/FileSystem.h"

namespace HachimiEngine
{
    std::filesystem::path AssetManager::s_AssetsDirectory;
    std::vector<Asset> AssetManager::s_Assets;
    std::unordered_map<std::string, Ref<Texture2D>> AssetManager::s_TextureCache;

    void AssetManager::Init(const std::filesystem::path& assetsDirectory)
    {
        s_AssetsDirectory = assetsDirectory;
        s_Assets.clear();
        s_TextureCache.clear();
        RefreshRegistry();
    }

    void AssetManager::Shutdown()
    {
        s_Assets.clear();
        s_TextureCache.clear();
    }

    void AssetManager::RefreshRegistry()
    {
        s_Assets.clear();

        if (!FileSystem::Exists(s_AssetsDirectory))
        {
            return;
        }

        const auto registerFiles = [&](const std::filesystem::path& directory, AssetType type)
        {
            for (const auto& file : FileSystem::GetFiles(directory))
            {
                Asset asset;
                asset.ID = UUID();
                asset.Type = type;
                asset.Path = file;
                asset.Name = FileSystem::GetFileNameWithoutExtension(file);
                s_Assets.push_back(asset);
            }
        };

        registerFiles(s_AssetsDirectory / "Textures", AssetType::Texture);
        registerFiles(s_AssetsDirectory / "Scenes", AssetType::Scene);
        registerFiles(s_AssetsDirectory / "Scripts", AssetType::Script);
    }

    Ref<Texture2D> AssetManager::GetTexture(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path fullPath = (s_AssetsDirectory / relativePath).lexically_normal();
        const std::string pathKey = fullPath.string();

        const auto cacheIt = s_TextureCache.find(pathKey);
        if (cacheIt != s_TextureCache.end())
        {
            return cacheIt->second;
        }

        if (!FileSystem::Exists(fullPath))
        {
            HE_CORE_ERROR("Texture asset does not exist: {}", fullPath.string());
            return nullptr;
        }

        const Ref<Texture2D> texture = TextureImporter::LoadTexture(fullPath);
        s_TextureCache[pathKey] = texture;
        return texture;
    }

    Ref<Texture2D> AssetManager::ImportTexture(const std::filesystem::path& sourcePath)
    {
        if (!FileSystem::Exists(sourcePath))
        {
            HE_CLIENT_ERROR("Cannot import texture, source does not exist: {}", sourcePath.string());
            return nullptr;
        }

        FileSystem::CreateDirectories(s_AssetsDirectory / "Textures");

        const std::filesystem::path destinationPath = s_AssetsDirectory / "Textures" / sourcePath.filename();
        if (!TextureImporter::ImportTexture(sourcePath, destinationPath))
        {
            HE_CLIENT_ERROR("Failed to copy texture into project Assets: {}", sourcePath.string());
            return nullptr;
        }

        const Ref<Texture2D> texture = TextureImporter::LoadTexture(destinationPath);
        s_TextureCache[destinationPath.string()] = texture;
        RefreshRegistry();
        return texture;
    }
}
