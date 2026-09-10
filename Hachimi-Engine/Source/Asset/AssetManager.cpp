#include "Asset/AssetManager.h"

#include "Asset/TextureImporter.h"
#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_set>

namespace HachimiEngine
{
    namespace
    {
        struct CompletedTextureRequest
        {
            std::filesystem::path RelativePath;
            std::filesystem::path FullPath;
            DecodedImage Image;
            Ref<Texture2D> CachedTexture;
            bool FromCache = false;
            bool Success = false;
            uint64_t RequestId = 0;
            AssetManager::TextureRequestCallback Callback;
        };

        std::atomic<uint64_t> s_NextRequestId{ 1 };
        std::mutex s_RequestMutex;
        std::unordered_set<uint64_t> s_OutstandingRequests;
        std::deque<CompletedTextureRequest> s_CompletedRequests;
    }

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
        // Dropping outstanding requests keeps no callback alive past teardown.
        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            s_OutstandingRequests.clear();
            s_CompletedRequests.clear();
        }

        s_Assets.clear();
        s_TextureCache.clear();
    }

    void AssetManager::RefreshRegistry()
    {
        s_Assets.clear();

        // A packaged "Assets" directory exists only as a set of package entries,
        // so existence and listings both go through the virtual file system.
        if (!VirtualFileSystem::Exists(s_AssetsDirectory))
        {
            return;
        }

        const auto registerFiles = [&](const std::filesystem::path& directory, AssetType type)
        {
            for (const auto& file : VirtualFileSystem::GetFiles(directory))
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

        if (!VirtualFileSystem::Exists(fullPath))
        {
            HE_CORE_ERROR("Texture asset does not exist: {}", fullPath.string());
            return nullptr;
        }

        const Ref<Texture2D> texture = TextureImporter::LoadTexture(fullPath);
        s_TextureCache[pathKey] = texture;
        return texture;
    }

    Ref<Texture2D> AssetManager::GetCachedTexture(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path fullPath = (s_AssetsDirectory / relativePath).lexically_normal();
        const auto cacheIt = s_TextureCache.find(fullPath.string());
        if (cacheIt == s_TextureCache.end())
        {
            return nullptr;
        }
        return cacheIt->second;
    }

    Ref<Texture2D> AssetManager::ImportTexture(const std::filesystem::path& sourcePath)
    {
        // Importing writes into the project directory, which a game package
        // cannot accept: it is read-only by construction.
        if (VirtualFileSystem::IsPackagedPath(s_AssetsDirectory))
        {
            HE_CLIENT_ERROR("Cannot import '{}': the active content root is a read-only game package",
                sourcePath.string());
            return nullptr;
        }

        if (!VirtualFileSystem::Exists(sourcePath))
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

    uint64_t AssetManager::RequestTexture(const std::filesystem::path& relativePath, TextureRequestCallback callback)
    {
        if (!callback)
        {
            return 0;
        }

        const std::filesystem::path fullPath = (s_AssetsDirectory / relativePath).lexically_normal();
        const uint64_t requestId = s_NextRequestId.fetch_add(1);

        // A cached texture needs no worker at all.
        const auto cached = s_TextureCache.find(fullPath.string());
        if (cached != s_TextureCache.end())
        {
            CompletedTextureRequest immediate;
            immediate.RelativePath = relativePath;
            immediate.FullPath = fullPath;
            immediate.CachedTexture = cached->second;
            immediate.FromCache = true;
            immediate.Success = cached->second != nullptr;
            immediate.RequestId = requestId;
            immediate.Callback = std::move(callback);

            std::lock_guard<std::mutex> lock(s_RequestMutex);
            s_CompletedRequests.push_back(std::move(immediate));
            return requestId;
        }

        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            s_OutstandingRequests.insert(requestId);
        }

        JobSystem::Submit([requestId, relativePath, fullPath, callback = std::move(callback)]() mutable
        {
            // Reading and decoding both stay off the main thread; only the GPU
            // upload is deferred to PumpCompletedRequests().
            std::vector<uint8_t> bytes;
            DecodedImage image;
            const bool success = VirtualFileSystem::ReadBinaryFile(fullPath, bytes)
                && ImageDecoder::DecodeFromMemory(bytes.data(), bytes.size(), image);

            std::lock_guard<std::mutex> lock(s_RequestMutex);
            if (s_OutstandingRequests.find(requestId) == s_OutstandingRequests.end())
            {
                // Dropped before the decode finished; no callback may fire.
                return;
            }

            CompletedTextureRequest completed;
            completed.RelativePath = relativePath;
            completed.FullPath = fullPath;
            completed.Image = std::move(image);
            completed.Success = success;
            completed.RequestId = requestId;
            completed.Callback = std::move(callback);
            s_CompletedRequests.push_back(std::move(completed));
        });

        return requestId;
    }

    void AssetManager::PumpCompletedRequests()
    {
        std::deque<CompletedTextureRequest> ready;
        {
            std::lock_guard<std::mutex> lock(s_RequestMutex);
            ready.swap(s_CompletedRequests);
        }

        for (CompletedTextureRequest& request : ready)
        {
            {
                std::lock_guard<std::mutex> lock(s_RequestMutex);
                const auto found = s_OutstandingRequests.find(request.RequestId);
                if (found == s_OutstandingRequests.end() && !request.FromCache)
                {
                    // Cancelled by Shutdown(); deliver nothing.
                    continue;
                }
                s_OutstandingRequests.erase(request.RequestId);
            }

            Ref<Texture2D> texture = request.CachedTexture;
            if (texture == nullptr && request.Success)
            {
                texture = Texture2D::Create(request.Image);
                s_TextureCache[request.FullPath.string()] = texture;
            }

            if (request.Callback)
            {
                request.Callback(request.RelativePath, texture);
            }
        }
    }

    size_t AssetManager::GetPendingRequestCount()
    {
        std::lock_guard<std::mutex> lock(s_RequestMutex);
        return s_OutstandingRequests.size();
    }
}
