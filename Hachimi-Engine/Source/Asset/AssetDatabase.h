#pragma once

#include "Asset/AssetHandle.h"
#include "Asset/AssetMeta.h"
#include "Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace HachimiEngine
{
    // What one scan of the asset tree did, so the editor can report it instead of silently
    // generating identities behind the user's back.
    struct AssetScanReport
    {
        size_t Scanned = 0;
        size_t Added = 0;
        size_t Removed = 0;
        size_t CreatedMeta = 0;
        size_t CorruptedMeta = 0;
        size_t Skipped = 0;
        // True when the scan could write sidecar files, i.e. the content root is loose files.
        bool WroteMetadata = false;
    };

    enum class AssetWriteResult
    {
        Success = 0,
        NotFound,
        AlreadyExists,
        ReadOnly,
        InvalidName,
        Failed
    };

    const char* ToString(AssetWriteResult result);

    // Index of everything under a project's Assets directory.
    //
    // It replaces the old static AssetManager registry, which minted a fresh UUID on every
    // refresh: an asset had no identity, so nothing could reference it. Here the identity lives
    // in a "<file>.meta" sidecar, the database is an ordinary object owned by the application,
    // and every path that changes a file (rename, move, delete, duplicate, import) goes through
    // this class so the sidecar and the index cannot drift apart.
    //
    // The database is a main-thread object. Reading and decoding asset bytes off the main thread
    // is TextureCache's job.
    class AssetDatabase
    {
    public:
        // Outcome of resolving a legacy path-only reference.
        struct EnsureResult
        {
            AssetHandle Handle;
            bool Created = false;
            bool Resolved = false;
        };

        void Clear();

        // Scans the content root and reconciles it with the sidecar files. Asset paths are read
        // through the virtual file system, so a packaged root works too; in that case every
        // write operation below is refused with AssetWriteResult::ReadOnly.
        bool Refresh(const std::filesystem::path& assetsDirectory);

        const std::filesystem::path& GetAssetsDirectory() const { return m_AssetsDirectory; }
        const AssetScanReport& GetLastScanReport() const { return m_ScanReport; }
        bool IsReadOnly() const { return m_ReadOnly; }

        const AssetMeta* GetMeta(AssetHandle handle) const;
        bool Contains(AssetHandle handle) const { return GetMeta(handle) != nullptr; }
        std::optional<AssetHandle> GetHandleForPath(const std::filesystem::path& assetPath) const;
        // Empty for an asset that is not indexed.
        std::filesystem::path GetAssetPath(AssetHandle handle) const;
        // File name without extension, or "<Missing>" when the asset is gone.
        std::string GetDisplayName(AssetHandle handle) const;
        // Reads the text of an asset file through the virtual file system.
        bool ReadAssetText(AssetHandle handle, std::string& outText) const;

        std::vector<AssetHandle> GetAllHandles(AssetType type) const;
        std::vector<AssetHandle> GetHandlesInDirectory(const std::filesystem::path& directory) const;

        // Sidecar-backed lookups used by the serializer when a scene stores a path or a UUID
        // that this project may not have indexed yet (a legacy scene, a loose .hscene).
        EnsureResult EnsureAsset(const std::filesystem::path& assetPath, AssetType type);
        // Adds or replaces the sidecar for an already known path and returns the handle.
        EnsureResult RegisterPath(const std::filesystem::path& assetPath, AssetType type, UUID preferredId = UUID::Invalid());
        // Removes an asset from the index without touching its files; used when a path was
        // registered for a file that turned out not to exist.
        void ForgetHandle(AssetHandle handle);

        AssetWriteResult CreateAsset(const std::filesystem::path& directory, const std::string& name,
                                     AssetType type, const std::string& textContent, AssetHandle& outHandle);
        AssetWriteResult ImportAsset(const std::filesystem::path& sourcePath,
                                     const std::filesystem::path& destinationDirectory, AssetHandle& outHandle);
        AssetWriteResult Rename(AssetHandle handle, const std::string& newName);
        AssetWriteResult Move(AssetHandle handle, const std::filesystem::path& destinationDirectory);
        AssetWriteResult Duplicate(AssetHandle handle, AssetHandle& outHandle);
        AssetWriteResult Delete(AssetHandle handle);
        AssetWriteResult WriteMeta(AssetHandle handle, const AssetMeta& meta);

        // Marks an asset as changed by someone who wrote its file directly, so caches keyed on the
        // revision - resolved materials, thumbnails - rebuild on their next use. Returns false when
        // the asset is not indexed.
        bool NotifyAssetChanged(AssetHandle handle);

        AssetWriteResult CreateDirectory(const std::filesystem::path& parent, const std::string& name,
                                         std::filesystem::path& outDirectory);
        AssetWriteResult RenameDirectory(const std::filesystem::path& directory, const std::string& newName,
                                         std::filesystem::path& outDirectory);
        // Removes a directory, every asset beneath it and every sidecar.
        AssetWriteResult DeleteDirectory(const std::filesystem::path& directory);

        // Paths that reference the asset: scenes whose components mention it and materials that
        // use it. Built during Refresh and refreshed on demand, so a delete can warn instead of
        // silently breaking a scene.
        std::vector<std::filesystem::path> FindReferences(AssetHandle handle);
        void RebuildReferenceIndex();

        // Bumped whenever the index changes, so caches keyed by asset (a resolved material, a
        // thumbnail request set) know to revalidate.
        uint64_t GetRevision() const { return m_Revision; }
        // Bumped per asset when the database re-reads it (a write through this class, or an
        // external change to the bytes on disk).
        uint64_t GetAssetRevision(AssetHandle handle) const;

    private:
        struct Entry
        {
            AssetMeta Meta;
            std::filesystem::path Path;
            std::filesystem::path FullPath;
            uint64_t Revision = 1;
            uint64_t FileSize = 0;
            std::filesystem::file_time_type LastWriteTime;
        };

        // What the previous scan knew about one path.
        //
        // A rescan clears the id indexes before it walks the tree, so this path-keyed record is the
        // only memory that survives: it is what tells an unchanged asset from one edited on disk,
        // and an existing identity from a new one.
        struct PathRecord
        {
            UUID ID = UUID::Invalid();
            uint64_t Revision = 0;
            uint64_t FileSize = 0;
            std::filesystem::file_time_type LastWriteTime;
        };

        void IndexEntry(Entry entry, bool countAsAdded);
        void RemoveEntry(AssetHandle handle, bool countAsRemoved);
        bool WriteMetadata(const Entry& entry) const;
        void TouchRevision(AssetHandle handle);
        // Re-reads an entry's size and modification time, so the next scan does not report the
        // database's own write as an external change.
        void RefreshFileStamp(Entry& entry);        void BumpRevision() { ++m_Revision; }
        bool IsWriteBlocked() const;
        // Shared tail of Rename and Move: relocates one file plus its sidecar and updates the
        // path indexes without touching the identity.
        AssetWriteResult MoveIntoPath(AssetHandle handle, const std::filesystem::path& destinationPath);

        std::filesystem::path ResolveFullPath(const std::filesystem::path& assetPath) const;
        static std::filesystem::path MakeRelative(const std::filesystem::path& path,
                                                  const std::filesystem::path& root);

        std::filesystem::path m_AssetsDirectory;
        std::unordered_map<UUID, Entry> m_EntriesById;
        std::unordered_map<std::string, UUID> m_IdsByPath;
        std::unordered_map<UUID, uint64_t> m_AssetRevisions;
        std::unordered_map<UUID, std::vector<std::filesystem::path>> m_References;
        std::unordered_map<std::string, UUID> m_GeneratedIds;
        // Sidecars a scan has to (re)write, collected while the entries are indexed so a write
        // failure cannot leave the index pointing at a record that was never stored.
        std::set<UUID> m_NeedsMetadataWrite;
        // Assets whose bytes changed since the previous scan, so their dependents can be told.
        std::set<UUID> m_ChangedOnDisk;
        // What the previous scan recorded per path. Survives a rescan; everything else does not.
        std::unordered_map<std::string, PathRecord> m_PathRecords;
        AssetScanReport m_ScanReport;
        bool m_ReadOnly = false;
        bool m_ReferencesDirty = true;
        uint64_t m_Revision = 1;
    };
}
