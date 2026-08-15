#pragma once

#include "Asset/Asset.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // Owns the project asset registry and the in-memory texture cache.
    class AssetManager
    {
    public:
        static void Init(const std::filesystem::path& assetsDirectory);
        static void Shutdown();

        static void RefreshRegistry();

        static const std::vector<Asset>& GetAssets() { return s_Assets; }
        static Ref<Texture2D> GetTexture(const std::filesystem::path& relativePath);
        static Ref<Texture2D> ImportTexture(const std::filesystem::path& sourcePath);

        static const std::filesystem::path& GetAssetsDirectory() { return s_AssetsDirectory; }

    private:
        static std::filesystem::path s_AssetsDirectory;
        static std::vector<Asset> s_Assets;
        static std::unordered_map<std::string, Ref<Texture2D>> s_TextureCache;
    };
}
