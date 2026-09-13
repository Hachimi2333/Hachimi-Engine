#include "Asset/TextureCache.h"

#include "Asset/AssetDatabase.h"
#include "Asset/TextureImporter.h"
#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        TextureSpecification MakeSpecification(const TextureImportSettings& settings)
        {
            TextureSpecification specification;
            specification.SRGB = settings.IsSRGB();
            specification.GenerateMips = settings.GenerateMipmaps;

            switch (settings.Wrap)
            {
                case TextureWrapMode::Clamp: specification.Address = TextureAddressMode::Clamp; break;
                case TextureWrapMode::MirroredRepeat: specification.Address = TextureAddressMode::MirroredRepeat; break;
                case TextureWrapMode::Repeat:
                default: specification.Address = TextureAddressMode::Repeat; break;
            }

            switch (settings.Filter)
            {
                case TextureFilterMode::Nearest: specification.Filter = TextureSamplingFilter::Nearest; break;
                case TextureFilterMode::Linear:
                default: specification.Filter = TextureSamplingFilter::Linear; break;
            }

            return specification;
        }

        bool SameSettings(const TextureImportSettings& lhs, const TextureImportSettings& rhs)
        {
            return lhs.Type == rhs.Type
                && lhs.GenerateMipmaps == rhs.GenerateMipmaps
                && lhs.Wrap == rhs.Wrap
                && lhs.Filter == rhs.Filter;
        }
    }

    TextureCache::~TextureCache()
    {
        // No callback may run after the cache is gone.
        CancelOutstanding();
    }

    void TextureCache::CancelOutstanding()
    {
        std::lock_guard<std::mutex> lock(m_RequestMutex);
        m_OutstandingRequests.clear();
        m_CompletedRequests.clear();
    }

    void TextureCache::Clear()
    {
        CancelOutstanding();
        m_Textures.clear();
    }

    void TextureCache::Store(AssetHandle handle, const Ref<Texture2D>& texture, const TextureImportSettings& settings)
    {
        if (texture == nullptr)
        {
            return;
        }

        CacheEntry entry;
        entry.Texture = texture;
        entry.Settings = settings;
        m_Textures[handle] = std::move(entry);
    }

    Ref<Texture2D> TextureCache::GetOrLoad(AssetHandle handle, const TextureImportSettings& settings)
    {
        const auto cached = m_Textures.find(handle);
        if (cached != m_Textures.end() && SameSettings(cached->second.Settings, settings))
        {
            return cached->second.Texture;
        }

        if (m_Database == nullptr)
        {
            return nullptr;
        }

        const std::filesystem::path path = m_Database->GetAssetPath(handle);
        if (path.empty())
        {
            HE_CORE_ERROR("Texture asset {} is not in the asset database", handle.ID.ToString());
            return nullptr;
        }

        Ref<Texture2D> texture = TextureImporter::LoadTexture(path, settings);
        if (texture == nullptr)
        {
            return nullptr;
        }

        Store(handle, texture, settings);
        return texture;
    }

    Ref<Texture2D> TextureCache::GetCached(AssetHandle handle) const
    {
        const auto cached = m_Textures.find(handle);
        return cached != m_Textures.end() ? cached->second.Texture : nullptr;
    }

    bool TextureCache::IsLoaded(AssetHandle handle) const
    {
        return m_Textures.contains(handle);
    }

    void TextureCache::Evict(AssetHandle handle)
    {
        m_Textures.erase(handle);
        // A request queued with the previous settings would store the stale result, so its
        // completion is dropped and the next GetOrLoad() re-reads the file.
        std::lock_guard<std::mutex> lock(m_RequestMutex);
        std::erase_if(m_CompletedRequests,
            [handle](const DecodedRequest& request) { return request.Handle == handle; });
    }

    uint64_t TextureCache::Request(AssetHandle handle, const TextureImportSettings& settings, TextureCallback callback)
    {
        if (!callback || m_Database == nullptr)
        {
            return 0;
        }

        const std::filesystem::path fullPath = m_Database->GetAssetPath(handle);
        if (fullPath.empty())
        {
            return 0;
        }

        const uint64_t requestId = m_NextRequestId.fetch_add(1);

        // A texture that is already uploaded needs no worker.
        const auto cached = m_Textures.find(handle);
        if (cached != m_Textures.end() && SameSettings(cached->second.Settings, settings))
        {
            DecodedRequest immediate;
            immediate.Handle = handle;
            immediate.FullPath = fullPath;
            immediate.Settings = settings;
            immediate.CachedTexture = cached->second.Texture;
            immediate.FromCache = true;
            immediate.Success = cached->second.Texture != nullptr;
            immediate.RequestId = requestId;
            immediate.Callback = std::move(callback);

            std::lock_guard<std::mutex> lock(m_RequestMutex);
            m_CompletedRequests.push_back(std::move(immediate));
            return requestId;
        }

        {
            std::lock_guard<std::mutex> lock(m_RequestMutex);
            m_OutstandingRequests.insert(requestId);
        }

        JobSystem::Submit([this, requestId, handle, fullPath, settings, callback = std::move(callback)]() mutable
        {
            // Reading and decoding stay off the main thread; only the GPU upload is deferred.
            std::vector<uint8_t> bytes;
            DecodedImage image;
            const bool success = VirtualFileSystem::ReadBinaryFile(fullPath, bytes)
                && ImageDecoder::DecodeFromMemory(bytes.data(), bytes.size(), image);

            std::lock_guard<std::mutex> lock(m_RequestMutex);
            if (!m_OutstandingRequests.contains(requestId))
            {
                // Cancelled by Clear() or the cache was destroyed; no callback may fire.
                return;
            }

            DecodedRequest completed;
            completed.Handle = handle;
            completed.FullPath = fullPath;
            completed.Settings = settings;
            completed.Image = std::move(image);
            completed.Success = success;
            completed.RequestId = requestId;
            completed.Callback = std::move(callback);
            m_CompletedRequests.push_back(std::move(completed));
        });

        return requestId;
    }

    void TextureCache::PumpCompletedRequests()
    {
        std::vector<DecodedRequest> ready;
        {
            std::lock_guard<std::mutex> lock(m_RequestMutex);
            ready.swap(m_CompletedRequests);
        }

        for (DecodedRequest& request : ready)
        {
            {
                std::lock_guard<std::mutex> lock(m_RequestMutex);
                const auto found = m_OutstandingRequests.find(request.RequestId);
                if (found == m_OutstandingRequests.end() && !request.FromCache)
                {
                    // Cancelled; deliver nothing.
                    continue;
                }
                m_OutstandingRequests.erase(request.RequestId);
            }

            Ref<Texture2D> texture = request.CachedTexture;
            if (texture == nullptr && request.Success)
            {
                texture = TextureImporter::UploadDecodedImage(request.Image, request.Settings);
                if (texture != nullptr)
                {
                    Store(request.Handle, texture, request.Settings);
                }
            }

            if (request.Callback)
            {
                request.Callback(request.Handle, texture);
            }
        }
    }

    size_t TextureCache::GetPendingRequestCount() const
    {
        std::lock_guard<std::mutex> lock(m_RequestMutex);
        return m_OutstandingRequests.size();
    }
}
