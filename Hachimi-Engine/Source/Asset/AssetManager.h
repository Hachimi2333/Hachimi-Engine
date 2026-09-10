#pragma once

#include "Asset/Asset.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/ImageDecoder.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // Owns the project asset registry and the in-memory texture cache.
    class AssetManager
    {
    public:
        // success is false when the texture is missing or cannot be decoded. The
        // callback runs on the main thread from PumpCompletedRequests().
        using TextureRequestCallback = std::function<void(const std::filesystem::path& relativePath,
                                                          const Ref<Texture2D>& texture)>;

        static void Init(const std::filesystem::path& assetsDirectory);
        static void Shutdown();

        static void RefreshRegistry();

        static const std::vector<Asset>& GetAssets() { return s_Assets; }
        static Ref<Texture2D> GetTexture(const std::filesystem::path& relativePath);
        // Cache-only lookup: returns nullptr instead of loading on a miss, so UI
        // code can poll for a texture that is still being decoded in the
        // background.
        static Ref<Texture2D> GetCachedTexture(const std::filesystem::path& relativePath);
        static Ref<Texture2D> ImportTexture(const std::filesystem::path& sourcePath);

        // Loads a texture on a worker thread: the read and the image decode both
        // happen off the main thread, and only the GPU upload is deferred to
        // PumpCompletedRequests(). Returns 0 when the request could not be queued.
        static uint64_t RequestTexture(const std::filesystem::path& relativePath, TextureRequestCallback callback);
        // Dispatches finished texture requests. Call once per frame.
        static void PumpCompletedRequests();
        static size_t GetPendingRequestCount();

        static const std::filesystem::path& GetAssetsDirectory() { return s_AssetsDirectory; }

    private:
        static std::filesystem::path s_AssetsDirectory;
        static std::vector<Asset> s_Assets;
        static std::unordered_map<std::string, Ref<Texture2D>> s_TextureCache;
    };
}
