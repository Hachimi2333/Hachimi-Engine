#pragma once

#include "Asset/AssetHandle.h"
#include "Asset/AssetMeta.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Texture.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HachimiEngine
{
    class AssetDatabase;

    // Resolved 2D textures, keyed by asset identity.
    //
    // Keys are UUIDs, and every entry remembers the import settings it was uploaded with, so a
    // rename keeps the texture alive (only the path changes) while flipping sRGB or the wrap mode
    // in the .meta sidecar can be detected and reloaded. This used to be the static cache inside
    // AssetManager, which was keyed by a relative path string and had no eviction at all.
    //
    // Threading: Request() reads and decodes on a worker thread and only defers the GPU upload to
    // PumpCompletedRequests(), which has to be called from the main thread. GetOrLoad() uploads
    // immediately and is therefore main-thread-only.
    class TextureCache
    {
    public:
        using TextureCallback = std::function<void(AssetHandle handle, const Ref<Texture2D>& texture)>;

        ~TextureCache();

        // Drops every cached texture and discards pending requests without firing callbacks.
        void Clear();

        // The database is the only path authority; nullptr disables loading.
        void SetDatabase(AssetDatabase* database) { m_Database = database; }

        // Main-thread load. Returns nullptr when the asset is unknown or cannot be decoded.
        Ref<Texture2D> GetOrLoad(AssetHandle handle, const TextureImportSettings& settings);
        // Cache-only lookup, for UI that keeps drawing a placeholder while a decode is in flight.
        Ref<Texture2D> GetCached(AssetHandle handle) const;
        // Reloads the texture with new settings, e.g. after the sidecar changed.
        void Evict(AssetHandle handle);
        bool IsLoaded(AssetHandle handle) const;

        // Worker-thread load. Returns 0 when the request could not be queued; the callback then
        // never runs, so a caller must treat 0 as "no request".
        uint64_t Request(AssetHandle handle, const TextureImportSettings& settings, TextureCallback callback);
        // Dispatches finished requests. Call once per frame from the main thread.
        void PumpCompletedRequests();
        size_t GetPendingRequestCount() const;

    private:
        struct DecodedRequest
        {
            AssetHandle Handle;
            std::filesystem::path FullPath;
            TextureImportSettings Settings;
            DecodedImage Image;
            Ref<Texture2D> CachedTexture;
            bool FromCache = false;
            bool Success = false;
            uint64_t RequestId = 0;
            TextureCallback Callback;
        };

        struct CacheEntry
        {
            Ref<Texture2D> Texture;
            TextureImportSettings Settings;
        };

        void Store(AssetHandle handle, const Ref<Texture2D>& texture, const TextureImportSettings& settings);
        void CancelOutstanding();

        AssetDatabase* m_Database = nullptr;
        std::unordered_map<AssetHandle, CacheEntry> m_Textures;

        mutable std::mutex m_RequestMutex;
        std::atomic<uint64_t> m_NextRequestId { 1 };
        std::unordered_set<uint64_t> m_OutstandingRequests;
        std::vector<DecodedRequest> m_CompletedRequests;
    };
}
