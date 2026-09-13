#include "Asset/AssetDatabase.h"

#include "Asset/MaterialAsset.h"
#include "Core/Log.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <string_view>
#include <unordered_set>

namespace HachimiEngine
{
    namespace
    {
        // YAML keys that carry an asset reference. Collecting every UUID-shaped scalar that
        // appears under one of these keys keeps the index simple: scenes and materials both
        // write references as "<key>: <uuid>", and a key that does not exist contributes nothing.
        constexpr std::string_view ReferenceKeys[] = { "Material", "Script", "AlbedoTexture" };

        bool IsUuidShaped(std::string_view text)
        {
            if (text.empty() || text.size() > 16)
            {
                return false;
            }

            return std::all_of(text.begin(), text.end(), [](unsigned char character)
            {
                return std::isxdigit(character) != 0;
            });
        }

        void CollectReferenceIds(const YAML::Node& node, std::unordered_set<UUID>& outIds)
        {
            if (!node)
            {
                return;
            }

            if (node.IsMap())
            {
                for (const auto& entry : node)
                {
                    if (!entry.first.IsScalar())
                    {
                        continue;
                    }

                    const std::string key = entry.first.as<std::string>();
                    const bool isReferenceKey = std::any_of(
                        std::begin(ReferenceKeys),
                        std::end(ReferenceKeys),
                        [&key](std::string_view candidate) { return candidate == key; });

                    if (isReferenceKey && entry.second.IsScalar())
                    {
                        const std::string value = entry.second.as<std::string>();
                        if (IsUuidShaped(value))
                        {
                            try
                            {
                                outIds.insert(UUID(std::stoull(value, nullptr, 16)));
                            }
                            catch (const std::exception&)
                            {
                                // Not a UUID after all; leave it out of the index.
                            }
                        }
                    }

                    CollectReferenceIds(entry.second, outIds);
                }
                return;
            }

            if (node.IsSequence())
            {
                for (const YAML::Node& element : node)
                {
                    CollectReferenceIds(element, outIds);
                }
            }
        }

        // Keeps a file name usable as a path component without silently mangling the user's text
        // into something they did not type.
        bool IsValidFileStem(const std::string& name)
        {
            if (name.empty() || name == "." || name == "..")
            {
                return false;
            }

            return name.find_first_of("\\/:*?\"<>|") == std::string::npos;
        }
    }

    const char* ToString(AssetWriteResult result)
    {
        switch (result)
        {
            case AssetWriteResult::Success: return "Success";
            case AssetWriteResult::NotFound: return "the asset does not exist";
            case AssetWriteResult::AlreadyExists: return "a file with that name already exists";
            case AssetWriteResult::ReadOnly: return "the content root is a read-only game package";
            case AssetWriteResult::InvalidName: return "the name is empty or contains invalid characters";
            case AssetWriteResult::Failed: return "the file operation failed";
            default: return "unknown";
        }
    }

    void AssetDatabase::Clear()
    {
        m_AssetsDirectory.clear();
        m_EntriesById.clear();
        m_IdsByPath.clear();
        m_AssetRevisions.clear();
        m_References.clear();
        m_GeneratedIds.clear();
        m_ScanReport = AssetScanReport();
        m_ReadOnly = false;
        m_ReferencesDirty = true;
        BumpRevision();
    }

    std::filesystem::path AssetDatabase::ResolveFullPath(const std::filesystem::path& assetPath) const
    {
        if (assetPath.is_absolute())
        {
            return assetPath.lexically_normal();
        }
        return (m_AssetsDirectory / assetPath).lexically_normal();
    }

    std::filesystem::path AssetDatabase::MakeRelative(const std::filesystem::path& path,
                                                      const std::filesystem::path& root)
    {
        std::error_code errorCode;
        const std::filesystem::path relative = std::filesystem::relative(path, root, errorCode);
        if (errorCode)
        {
            return path.lexically_normal();
        }
        return relative.lexically_normal();
    }

    bool AssetDatabase::Refresh(const std::filesystem::path& assetsDirectory)
    {
        m_AssetsDirectory = assetsDirectory.lexically_normal();
        m_EntriesById.clear();
        m_IdsByPath.clear();
        m_GeneratedIds.clear();
        m_References.clear();
        m_ReferencesDirty = true;
        m_ScanReport = AssetScanReport();
        BumpRevision();

        if (m_AssetsDirectory.empty() || !VirtualFileSystem::Exists(m_AssetsDirectory))
        {
            HE_CORE_WARN("Asset database has no content root: {}", m_AssetsDirectory.string());
            return false;
        }

        // A packaged content root is read-only by construction: sidecars ship inside the
        // package and importing into it is impossible.
        m_ReadOnly = VirtualFileSystem::IsPackagedPath(m_AssetsDirectory);
        m_ScanReport.WroteMetadata = !m_ReadOnly;

        for (const std::filesystem::path& filePath : VirtualFileSystem::GetFilesRecursive(m_AssetsDirectory))
        {
            if (IsAssetMetaPath(filePath))
            {
                continue;
            }

            const AssetType type = GetAssetTypeForExtension(filePath.extension().string());
            if (type == AssetType::None)
            {
                ++m_ScanReport.Skipped;
                continue;
            }

            ++m_ScanReport.Scanned;

            const std::filesystem::path fullPath = ResolveFullPath(filePath);
            const std::filesystem::path relativePath = MakeRelative(fullPath, m_AssetsDirectory);

            // What the previous scan recorded for this path: its identity, its revision and the
            // stamp of its bytes. Keyed by path because a rescan clears the id indexes first, so a
            // scan that finds nothing changed leaves every dependent cache alone while a hand-edited
            // file invalidates them.
            const auto knownPath = m_PathRecords.find(relativePath.generic_string());
            const PathRecord previous = knownPath != m_PathRecords.end() ? knownPath->second : PathRecord();

            Entry entry;
            entry.Path = relativePath;
            entry.FullPath = fullPath;
            entry.FileSize = VirtualFileSystem::GetFileSize(fullPath);
            if (fullPath.is_absolute())
            {
                entry.LastWriteTime = FileSystem::GetLastWriteTime(fullPath);
            }


            const std::filesystem::path metaPath = GetAssetMetaPath(fullPath);
            std::string metaText;
            const bool hasMeta = VirtualFileSystem::ReadTextFile(metaPath, metaText);

            // A record that was generated or that could not be used has to reach the disk, or the
            // next load would mint a different identity.
            bool needsWrite = !hasMeta;

            if (hasMeta)
            {
                AssetMeta meta;
                if (AssetMeta::Deserialize(metaText, meta))
                {
                    entry.Meta = meta;
                }
                else
                {
                    HE_CORE_ERROR("Asset metadata for '{}' could not be read; regenerating its identity",
                        relativePath.generic_string());
                    ++m_ScanReport.CorruptedMeta;
                    entry.Meta = AssetMeta::MakeDefault(type);
                    needsWrite = true;
                }
            }
            else
            {
                entry.Meta = AssetMeta::MakeDefault(type);
                ++m_ScanReport.CreatedMeta;
            }

            // The record's declared kind must match the file it sits next to; a mismatch is a
            // hand-edited or copied sidecar, and the file itself wins.
            if (entry.Meta.Type != type)
            {
                HE_CORE_WARN("Asset metadata for '{}' declares type {} but the file is {}; using the file",
                    relativePath.generic_string(),
                    static_cast<int>(entry.Meta.Type),
                    static_cast<int>(type));
                entry.Meta.Type = type;
                needsWrite = true;
            }

            // A readable record can still carry an unusable identity, which is repaired the same way
            // as a record that could not be parsed at all.
            if (entry.Meta.ID == UUID::Invalid())
            {
                HE_CORE_ERROR("Asset metadata for '{}' has no usable identity; regenerating it",
                    relativePath.generic_string());
                ++m_ScanReport.CorruptedMeta;
                entry.Meta.ID = UUID();
                needsWrite = true;
            }

            // Two files can end up claiming one UUID (a copied .meta file). The second one is
            // re-identified rather than silently shadowing the first.
            if (m_EntriesById.contains(entry.Meta.ID))
            {
                HE_CORE_ERROR("Asset '{}' and '{}' both claim UUID {}; re-identifying the second",
                    m_EntriesById.at(entry.Meta.ID).Path.generic_string(),
                    relativePath.generic_string(),
                    entry.Meta.ID.ToString());
                entry.Meta.ID = UUID();
                entry.Meta.Type = type;
                ++m_ScanReport.CorruptedMeta;
                needsWrite = true;
            }

            if (needsWrite)
            {
                m_NeedsMetadataWrite.insert(entry.Meta.ID);
            }

            // A file behind a package entry has no useful stamp, and one that was just identified
            // has nothing to compare against.
            const bool canCompareStamp = fullPath.is_absolute()
                && !VirtualFileSystem::IsPackagedPath(fullPath)
                && previous.Revision != 0;
            const bool fileChanged = canCompareStamp
                && (previous.FileSize != entry.FileSize || previous.LastWriteTime != entry.LastWriteTime);

            if (fileChanged)
            {
                HE_CORE_INFO("Asset '{}' changed on disk; its dependents are invalidated",
                    relativePath.generic_string());
            }

            // A record that was generated or repaired has to reach the disk, or the next load would
            // mint a different identity. Sidecars whose bytes are already correct are left alone, so
            // a scan does not rewrite every record on every project open.
            if (needsWrite)
            {
                m_NeedsMetadataWrite.insert(entry.Meta.ID);
            }

            if (fileChanged)
            {
                m_ChangedOnDisk.insert(entry.Meta.ID);
            }

            IndexEntry(std::move(entry), needsWrite);
        }

        // Revisions of assets that disappeared are dropped, so a project that deletes and
        // re-adds content does not accumulate counters forever.
        for (auto it = m_AssetRevisions.begin(); it != m_AssetRevisions.end();)
        {
            it = m_EntriesById.contains(it->first) ? std::next(it) : m_AssetRevisions.erase(it);
        }

        // Bumping the revision of a file that changed underneath the project is what makes an open
        // editor pick up a hand-edited material or a re-exported texture.
        for (const UUID id : m_ChangedOnDisk)
        {
            ++m_AssetRevisions[id];
            if (const auto entry = m_EntriesById.find(id); entry != m_EntriesById.end())
            {
                entry->second.Revision = m_AssetRevisions[id];
            }
        }
        m_ChangedOnDisk.clear();

        if (!m_ReadOnly)
        {
            for (const UUID id : m_NeedsMetadataWrite)
            {
                const auto entry = m_EntriesById.find(id);
                if (entry != m_EntriesById.end())
                {
                    WriteMetadata(entry->second);
                }
            }
        }
        m_NeedsMetadataWrite.clear();

        HE_CORE_INFO("Asset database scanned {} file(s) under {}: {} indexed, {} metadata created, {} repaired, {} skipped{}",
            m_ScanReport.Scanned,
            m_AssetsDirectory.string(),
            m_EntriesById.size(),
            m_ScanReport.CreatedMeta,
            m_ScanReport.CorruptedMeta,
            m_ScanReport.Skipped,
            m_ReadOnly ? " (read-only package)" : "");

        return true;
    }

    void AssetDatabase::IndexEntry(Entry entry, bool countAsAdded)
    {
        const UUID id = entry.Meta.ID;
        const std::string pathKey = entry.Path.generic_string();

        // A re-scan of an unchanged asset keeps the revision it already had, so a refresh does
        // not invalidate every thumbnail and every resolved material.
        const auto previousRevision = m_AssetRevisions.find(id);
        entry.Revision = previousRevision != m_AssetRevisions.end() ? previousRevision->second : 1;

        m_IdsByPath[pathKey] = id;
        m_AssetRevisions[id] = entry.Revision;

        // The path record is the only thing that survives a rescan, so it carries everything the
        // next scan needs to tell "unchanged" from "edited on disk" and "same identity" from "new".
        PathRecord& record = m_PathRecords[pathKey];
        record.ID = id;
        record.Revision = entry.Revision;
        record.FileSize = entry.FileSize;
        record.LastWriteTime = entry.LastWriteTime;

        if (countAsAdded)
        {
            m_GeneratedIds[pathKey] = id;
            ++m_ScanReport.Added;
        }

        m_EntriesById[id] = std::move(entry);
    }

    void AssetDatabase::RemoveEntry(AssetHandle handle, bool countAsRemoved)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end())
        {
            return;
        }

        m_IdsByPath.erase(found->second.Path.generic_string());
        m_PathRecords.erase(found->second.Path.generic_string());
        m_EntriesById.erase(found);
        m_ReferencesDirty = true;
        BumpRevision();

        if (countAsRemoved)
        {
            ++m_ScanReport.Removed;
        }
    }

    const AssetMeta* AssetDatabase::GetMeta(AssetHandle handle) const
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }

        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return nullptr;
        }

        return &found->second.Meta;
    }

    std::optional<AssetHandle> AssetDatabase::GetHandleForPath(const std::filesystem::path& assetPath) const
    {
        const std::filesystem::path fullPath = ResolveFullPath(assetPath);
        const std::string pathKey = MakeRelative(fullPath, m_AssetsDirectory).generic_string();

        const auto found = m_IdsByPath.find(pathKey);
        if (found == m_IdsByPath.end())
        {
            return std::nullopt;
        }

        const auto entry = m_EntriesById.find(found->second);
        if (entry == m_EntriesById.end())
        {
            return std::nullopt;
        }

        return AssetHandle::From(entry->second.Meta.ID, entry->second.Meta.Type);
    }

    std::filesystem::path AssetDatabase::GetAssetPath(AssetHandle handle) const
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return {};
        }
        return found->second.FullPath;
    }

    std::string AssetDatabase::GetDisplayName(AssetHandle handle) const
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return "<Missing>";
        }
        return found->second.Path.stem().string();
    }

    bool AssetDatabase::ReadAssetText(AssetHandle handle, std::string& outText) const
    {
        const std::filesystem::path path = GetAssetPath(handle);
        if (path.empty())
        {
            return false;
        }
        return VirtualFileSystem::ReadTextFile(path, outText);
    }

    std::vector<AssetHandle> AssetDatabase::GetAllHandles(AssetType type) const
    {
        std::vector<AssetHandle> handles;
        for (const auto& [id, entry] : m_EntriesById)
        {
            if (entry.Meta.Type == type)
            {
                handles.push_back(AssetHandle::From(id, type));
            }
        }

        std::sort(handles.begin(), handles.end(), [this](const AssetHandle& lhs, const AssetHandle& rhs)
        {
            const auto left = m_EntriesById.find(lhs.ID);
            const auto right = m_EntriesById.find(rhs.ID);
            return left->second.Path.generic_string() < right->second.Path.generic_string();
        });

        return handles;
    }

    std::vector<AssetHandle> AssetDatabase::GetHandlesInDirectory(const std::filesystem::path& directory) const
    {
        const std::filesystem::path fullDirectory = ResolveFullPath(directory);
        const std::string directoryKey = MakeRelative(fullDirectory, m_AssetsDirectory).generic_string();

        std::vector<AssetHandle> handles;
        for (const auto& [id, entry] : m_EntriesById)
        {
            const std::filesystem::path parentKey = MakeRelative(entry.Path.parent_path(), m_AssetsDirectory);
            if (parentKey.generic_string() == directoryKey)
            {
                handles.push_back(AssetHandle::From(id, entry.Meta.Type));
            }
        }

        std::sort(handles.begin(), handles.end(), [this](const AssetHandle& lhs, const AssetHandle& rhs)
        {
            const auto left = m_EntriesById.find(lhs.ID);
            const auto right = m_EntriesById.find(rhs.ID);
            return left->second.Path.generic_string() < right->second.Path.generic_string();
        });

        return handles;
    }

    uint64_t AssetDatabase::GetAssetRevision(AssetHandle handle) const
    {
        const auto found = m_AssetRevisions.find(handle.ID);
        return found != m_AssetRevisions.end() ? found->second : 0;
    }

    void AssetDatabase::TouchRevision(AssetHandle handle)
    {
        ++m_AssetRevisions[handle.ID];

        const auto entry = m_EntriesById.find(handle.ID);
        if (entry != m_EntriesById.end())
        {
            entry->second.Revision = m_AssetRevisions[handle.ID];
        }
    }

    void AssetDatabase::RefreshFileStamp(Entry& entry)
    {
        entry.FileSize = VirtualFileSystem::GetFileSize(entry.FullPath);
        if (entry.FullPath.is_absolute())
        {
            entry.LastWriteTime = FileSystem::GetLastWriteTime(entry.FullPath);
        }

        // The path record is what the next scan compares against, so a write this class made must
        // not come back as an external edit.
        PathRecord& record = m_PathRecords[entry.Path.generic_string()];
        record.ID = entry.Meta.ID;
        record.Revision = entry.Revision;
        record.FileSize = entry.FileSize;
        record.LastWriteTime = entry.LastWriteTime;
    }

    bool AssetDatabase::IsWriteBlocked() const
    {
        if (m_ReadOnly)
        {
            HE_CLIENT_ERROR("The active content root is a read-only game package; asset changes are refused");
            return true;
        }
        if (m_AssetsDirectory.empty())
        {
            HE_CLIENT_ERROR("No asset database is loaded; asset changes are refused");
            return true;
        }
        return false;
    }

    bool AssetDatabase::WriteMetadata(const Entry& entry) const
    {
        if (m_ReadOnly)
        {
            return false;
        }

        AssetMeta meta = entry.Meta;
        const std::filesystem::path metaPath = GetAssetMetaPath(entry.FullPath);
        if (!FileSystem::WriteTextFile(metaPath, AssetMeta::Serialize(meta)))
        {
            HE_CORE_ERROR("Failed to write asset metadata: {}", metaPath.string());
            return false;
        }
        return true;
    }

    AssetDatabase::EnsureResult AssetDatabase::EnsureAsset(const std::filesystem::path& assetPath, AssetType type)
    {
        EnsureResult result;

        if (const std::optional<AssetHandle> existing = GetHandleForPath(assetPath))
        {
            result.Handle = *existing;
            result.Resolved = true;
            return result;
        }

        const std::filesystem::path fullPath = ResolveFullPath(assetPath);
        if (!VirtualFileSystem::Exists(fullPath))
        {
            return result;
        }

        // A file the scan never reached (a scene or material written at runtime): give it an
        // identity now so a reference to it has something stable to point at.
        result = RegisterPath(fullPath, type);
        result.Created = result.Handle.IsValid();
        return result;
    }

    AssetDatabase::EnsureResult AssetDatabase::RegisterPath(const std::filesystem::path& assetPath, AssetType type,
                                                            UUID preferredId)
    {
        EnsureResult result;

        const std::filesystem::path fullPath = ResolveFullPath(assetPath);
        const std::filesystem::path relativePath = MakeRelative(fullPath, m_AssetsDirectory);
        const std::string pathKey = relativePath.generic_string();

        if (const auto existing = m_IdsByPath.find(pathKey); existing != m_IdsByPath.end())
        {
            const auto entry = m_EntriesById.find(existing->second);
            if (entry != m_EntriesById.end())
            {
                result.Handle = AssetHandle::From(entry->second.Meta.ID, entry->second.Meta.Type);
                result.Resolved = true;
                return result;
            }
        }

        Entry entry;
        entry.Path = relativePath;
        entry.FullPath = fullPath;
        entry.Meta = AssetMeta::MakeDefault(type);
        entry.Meta.Type = type;

        std::string metaText;
        if (VirtualFileSystem::ReadTextFile(GetAssetMetaPath(fullPath), metaText))
        {
            AssetMeta stored;
            if (AssetMeta::Deserialize(metaText, stored) && stored.Type == type)
            {
                entry.Meta = stored;
            }
            else
            {
                HE_CORE_ERROR("Stored asset metadata for '{}' is unusable; generating a new identity",
                    pathKey);
            }
        }

        if (preferredId != UUID::Invalid() && m_EntriesById.find(preferredId) == m_EntriesById.end())
        {
            entry.Meta.ID = preferredId;
        }

        if (m_EntriesById.find(entry.Meta.ID) != m_EntriesById.end())
        {
            entry.Meta.ID = UUID();
        }

        if (!m_ReadOnly)
        {
            WriteMetadata(entry);
        }

        result.Handle = AssetHandle::From(entry.Meta.ID, type);
        BumpRevision();
        IndexEntry(std::move(entry), true);
        return result;
    }

    void AssetDatabase::ForgetHandle(AssetHandle handle)
    {
        RemoveEntry(handle, true);
    }

    AssetWriteResult AssetDatabase::CreateAsset(const std::filesystem::path& directory, const std::string& name,
                                                AssetType type, const std::string& textContent, AssetHandle& outHandle)
    {
        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        if (!IsValidFileStem(name))
        {
            return AssetWriteResult::InvalidName;
        }

        const std::filesystem::path fullDirectory = ResolveFullPath(directory);
        if (!FileSystem::CreateDirectories(fullDirectory))
        {
            return AssetWriteResult::Failed;
        }

        std::filesystem::path assetPath = fullDirectory / name;
        if (assetPath.extension().empty())
        {
            // Callers pass a bare name; give it the extension of the kind being created.
            switch (type)
            {
                case AssetType::Material: assetPath += MaterialAsset::FileExtension; break;
                case AssetType::Scene: assetPath += ".hscene"; break;
                case AssetType::Script: assetPath += ".lua"; break;
                default: break;
            }
        }

        if (FileSystem::Exists(assetPath))
        {
            return AssetWriteResult::AlreadyExists;
        }

        if (!FileSystem::WriteTextFile(assetPath, textContent))
        {
            return AssetWriteResult::Failed;
        }

        EnsureResult registered = RegisterPath(assetPath, type);
        if (!registered.Handle.IsValid())
        {
            return AssetWriteResult::Failed;
        }

        outHandle = registered.Handle;
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::ImportAsset(const std::filesystem::path& sourcePath,
                                                const std::filesystem::path& destinationDirectory,
                                                AssetHandle& outHandle)
    {
        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        if (!FileSystem::Exists(sourcePath))
        {
            HE_CLIENT_ERROR("Cannot import '{}': the source does not exist", sourcePath.string());
            return AssetWriteResult::NotFound;
        }

        const AssetType type = GetAssetTypeForExtension(sourcePath.extension().string());
        if (type == AssetType::None)
        {
            HE_CLIENT_ERROR("Cannot import '{}': its file type is not an asset kind", sourcePath.string());
            return AssetWriteResult::Failed;
        }

        const std::filesystem::path fullDirectory = ResolveFullPath(destinationDirectory);
        if (!FileSystem::CreateDirectories(fullDirectory))
        {
            return AssetWriteResult::Failed;
        }

        // A name collision gets a numeric suffix: overwriting a project asset because two files
        // in different folders share a name loses work silently.
        std::filesystem::path destinationPath = fullDirectory / sourcePath.filename();
        const std::string stem = destinationPath.stem().string();
        const std::string extension = destinationPath.extension().string();
        for (int suffix = 1; FileSystem::Exists(destinationPath); ++suffix)
        {
            if (suffix > 999)
            {
                return AssetWriteResult::AlreadyExists;
            }
            destinationPath = fullDirectory / (stem + "_" + std::to_string(suffix) + extension);
        }

        if (!FileSystem::CopyFile(sourcePath, destinationPath))
        {
            HE_CLIENT_ERROR("Failed to copy '{}' into the project", sourcePath.string());
            return AssetWriteResult::Failed;
        }

        EnsureResult registered = RegisterPath(destinationPath, type);
        if (!registered.Handle.IsValid())
        {
            return AssetWriteResult::Failed;
        }

        outHandle = registered.Handle;
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::Rename(AssetHandle handle, const std::string& newName)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return AssetWriteResult::NotFound;
        }

        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        // The sidecar is keyed by the full file name, so keeping the original extension intact
        // is what keeps ".png.meta" attached; a name typed with the extension is accepted.
        const std::filesystem::path& currentPath = found->second.Path;
        const std::string extension = currentPath.extension().string();
        std::string stem = newName;
        if (!extension.empty()
            && stem.size() > extension.size()
            && stem.compare(stem.size() - extension.size(), extension.size(), extension) == 0)
        {
            stem = stem.substr(0, stem.size() - extension.size());
        }

        if (!IsValidFileStem(stem))
        {
            return AssetWriteResult::InvalidName;
        }

        const std::filesystem::path destinationPath = currentPath.parent_path() / (stem + extension);
        if (destinationPath == currentPath)
        {
            return AssetWriteResult::Success;
        }

        return MoveIntoPath(handle, destinationPath);
    }

    AssetWriteResult AssetDatabase::MoveIntoPath(AssetHandle handle, const std::filesystem::path& destinationPath)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end())
        {
            return AssetWriteResult::NotFound;
        }

        const std::filesystem::path sourcePath = found->second.FullPath;
        const std::filesystem::path fullDestination = ResolveFullPath(destinationPath);

        if (fullDestination == sourcePath)
        {
            return AssetWriteResult::Success;
        }

        if (FileSystem::Exists(fullDestination))
        {
            return AssetWriteResult::AlreadyExists;
        }

        if (!FileSystem::CreateDirectories(fullDestination.parent_path()))
        {
            return AssetWriteResult::Failed;
        }

        if (!FileSystem::MoveFile(sourcePath, fullDestination))
        {
            HE_CLIENT_ERROR("Failed to move '{}' to '{}'", sourcePath.string(), fullDestination.string());
            return AssetWriteResult::Failed;
        }

        const std::filesystem::path sourceMeta = GetAssetMetaPath(sourcePath);
        if (FileSystem::Exists(sourceMeta))
        {
            FileSystem::MoveFile(sourceMeta, GetAssetMetaPath(fullDestination));
        }

        // The identity travels with the file, so every existing reference keeps working; only the
        // two path indexes have to be updated.
        const std::filesystem::path relativePath = MakeRelative(fullDestination, m_AssetsDirectory);
        m_IdsByPath.erase(found->second.Path.generic_string());
        found->second.Path = relativePath;
        found->second.FullPath = fullDestination;
        m_IdsByPath[relativePath.generic_string()] = handle.ID;
        RefreshFileStamp(found->second);

        m_ReferencesDirty = true;
        TouchRevision(handle);
        BumpRevision();
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::Move(AssetHandle handle, const std::filesystem::path& destinationDirectory)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return AssetWriteResult::NotFound;
        }

        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        const std::filesystem::path sourcePath = found->second.FullPath;
        const std::filesystem::path fullDirectory = ResolveFullPath(destinationDirectory);

        std::filesystem::path destinationPath = fullDirectory / sourcePath.filename();
        const std::string stem = destinationPath.stem().string();
        const std::string extension = destinationPath.extension().string();

        // Moving into a folder that already holds a file of that name keeps both: the incoming
        // one is renamed rather than replacing the resident file.
        for (int suffix = 1; FileSystem::Exists(destinationPath) && destinationPath != sourcePath; ++suffix)
        {
            if (suffix > 999)
            {
                return AssetWriteResult::AlreadyExists;
            }
            destinationPath = fullDirectory / (stem + "_" + std::to_string(suffix) + extension);
        }

        return MoveIntoPath(handle, destinationPath);
    }

    AssetWriteResult AssetDatabase::Duplicate(AssetHandle handle, AssetHandle& outHandle)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return AssetWriteResult::NotFound;
        }

        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        const std::filesystem::path sourcePath = found->second.FullPath;
        const std::filesystem::path directory = sourcePath.parent_path();
        const std::string stem = sourcePath.stem().string();
        const std::string extension = sourcePath.extension().string();

        std::filesystem::path destinationPath = directory / (stem + "_Copy" + extension);
        for (int suffix = 1; FileSystem::Exists(destinationPath); ++suffix)
        {
            destinationPath = directory / (stem + "_Copy" + std::to_string(suffix) + extension);
        }

        if (!FileSystem::CopyFile(sourcePath, destinationPath))
        {
            return AssetWriteResult::Failed;
        }

        // A duplicate is a new asset: it gets its own identity so editing one copy never
        // changes the other. Only the sidecar is not copied, which is exactly the intent.
        EnsureResult registered = RegisterPath(destinationPath, handle.Type);
        if (!registered.Handle.IsValid())
        {
            return AssetWriteResult::Failed;
        }

        outHandle = registered.Handle;
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::Delete(AssetHandle handle)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return AssetWriteResult::NotFound;
        }

        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        const std::filesystem::path assetPath = found->second.FullPath;
        if (!FileSystem::RemoveAll(assetPath))
        {
            HE_CLIENT_ERROR("Failed to delete asset file: {}", assetPath.string());
            return AssetWriteResult::Failed;
        }

        FileSystem::RemoveAll(GetAssetMetaPath(assetPath));
        RemoveEntry(handle, true);
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::WriteMeta(AssetHandle handle, const AssetMeta& meta)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return AssetWriteResult::NotFound;
        }

        if (!meta.ID.IsValid())
        {
            return AssetWriteResult::InvalidName;
        }

        // Rewriting the sidecar under a different UUID would break every reference, so the
        // identity is fixed here and only the descriptive fields are replaced.
        AssetMeta updated = meta;
        updated.ID = handle.ID;
        updated.Type = handle.Type;
        found->second.Meta = updated;
        m_GeneratedIds[found->second.Path.generic_string()] = handle.ID;

        if (!WriteMetadata(found->second))
        {
            return AssetWriteResult::Failed;
        }

        m_ReferencesDirty = true;
        TouchRevision(handle);
        BumpRevision();
        return AssetWriteResult::Success;
    }

    bool AssetDatabase::NotifyAssetChanged(AssetHandle handle)
    {
        const auto found = m_EntriesById.find(handle.ID);
        if (found == m_EntriesById.end() || found->second.Meta.Type != handle.Type)
        {
            return false;
        }

        // The writer already touched the file, so only the bookkeeping is left.
        RefreshFileStamp(found->second);
        TouchRevision(handle);
        BumpRevision();
        return true;
    }

    AssetWriteResult AssetDatabase::CreateDirectory(const std::filesystem::path& parent, const std::string& name,
                                                    std::filesystem::path& outDirectory)    {
        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        if (!IsValidFileStem(name))
        {
            return AssetWriteResult::InvalidName;
        }

        const std::filesystem::path directory = ResolveFullPath(parent) / name;
        if (FileSystem::Exists(directory))
        {
            return AssetWriteResult::AlreadyExists;
        }

        if (!FileSystem::CreateDirectories(directory))
        {
            return AssetWriteResult::Failed;
        }

        outDirectory = directory;
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::RenameDirectory(const std::filesystem::path& directory, const std::string& newName,
                                                     std::filesystem::path& outDirectory)
    {
        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        if (!IsValidFileStem(newName))
        {
            return AssetWriteResult::InvalidName;
        }

        const std::filesystem::path sourceDirectory = ResolveFullPath(directory);
        if (!FileSystem::IsDirectory(sourceDirectory))
        {
            return AssetWriteResult::NotFound;
        }

        if (sourceDirectory == m_AssetsDirectory)
        {
            return AssetWriteResult::Failed;
        }

        const std::filesystem::path destinationDirectory = sourceDirectory.parent_path() / newName;
        if (FileSystem::Exists(destinationDirectory))
        {
            return AssetWriteResult::AlreadyExists;
        }

        if (!FileSystem::MoveFile(sourceDirectory, destinationDirectory))
        {
            return AssetWriteResult::Failed;
        }

        // Every file beneath the directory - and its sidecar, which sits next to it - moved with
        // it, so only the path index has to be rebuilt. Identities are untouched.
        std::vector<std::pair<UUID, std::filesystem::path>> moved;
        moved.reserve(m_EntriesById.size());
        for (auto& [id, entry] : m_EntriesById)
        {
            std::error_code errorCode;
            const std::filesystem::path relative = std::filesystem::relative(entry.FullPath, sourceDirectory, errorCode);
            if (errorCode || relative.empty() || relative.native().starts_with(L".."))
            {
                continue;
            }
            moved.emplace_back(id, destinationDirectory / relative);
        }

        for (auto& [id, newPath] : moved)
        {
            auto entry = m_EntriesById.find(id);
            if (entry == m_EntriesById.end())
            {
                continue;
            }

            m_IdsByPath.erase(entry->second.Path.generic_string());
            entry->second.FullPath = newPath;
            entry->second.Path = MakeRelative(newPath, m_AssetsDirectory);
            m_IdsByPath[entry->second.Path.generic_string()] = id;
        }

        outDirectory = destinationDirectory;
        m_ReferencesDirty = true;
        BumpRevision();
        return AssetWriteResult::Success;
    }

    AssetWriteResult AssetDatabase::DeleteDirectory(const std::filesystem::path& directory)
    {
        if (IsWriteBlocked())
        {
            return AssetWriteResult::ReadOnly;
        }

        const std::filesystem::path fullDirectory = ResolveFullPath(directory);
        if (fullDirectory == m_AssetsDirectory || !FileSystem::IsDirectory(fullDirectory))
        {
            return AssetWriteResult::NotFound;
        }

        if (!FileSystem::RemoveAll(fullDirectory))
        {
            HE_CLIENT_ERROR("Failed to delete directory: {}", fullDirectory.string());
            return AssetWriteResult::Failed;
        }

        std::vector<AssetHandle> removed;
        for (const auto& [id, entry] : m_EntriesById)
        {
            std::error_code errorCode;
            const std::filesystem::path relative = std::filesystem::relative(entry.FullPath, fullDirectory, errorCode);
            if (!errorCode && !relative.empty() && !relative.native().starts_with(L".."))
            {
                removed.push_back(AssetHandle::From(id, entry.Meta.Type));
            }
        }

        for (const AssetHandle handle : removed)
        {
            RemoveEntry(handle, true);
        }

        m_ReferencesDirty = true;
        BumpRevision();
        return AssetWriteResult::Success;
    }

    void AssetDatabase::RebuildReferenceIndex()
    {
        m_References.clear();

        for (const auto& [id, entry] : m_EntriesById)
        {
            (void)id;

            // Only documents that can name another asset are parsed; textures and scripts are
            // leaves and are skipped without a read.
            if (entry.Meta.Type != AssetType::Scene && entry.Meta.Type != AssetType::Material)
            {
                continue;
            }

            std::string text;
            if (!VirtualFileSystem::ReadTextFile(entry.FullPath, text))
            {
                continue;
            }

            YAML::Node data;
            try
            {
                data = YAML::Load(text);
            }
            catch (const YAML::Exception&)
            {
                // A scene that does not parse cannot be depended on for references; the loader
                // reports it when it is opened.
                continue;
            }

            std::unordered_set<UUID> referenced;
            CollectReferenceIds(data, referenced);

            for (const UUID referencedId : referenced)
            {
                if (m_EntriesById.find(referencedId) != m_EntriesById.end())
                {
                    m_References[referencedId].push_back(entry.Path);
                }
            }
        }

        for (auto& [id, paths] : m_References)
        {
            (void)id;
            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
        }

        m_ReferencesDirty = false;
    }

    std::vector<std::filesystem::path> AssetDatabase::FindReferences(AssetHandle handle)
    {
        if (m_ReferencesDirty)
        {
            RebuildReferenceIndex();
        }

        const auto found = m_References.find(handle.ID);
        if (found == m_References.end())
        {
            return {};
        }

        // The asset's own document is not a reference to itself.
        std::vector<std::filesystem::path> references = found->second;
        if (const auto entry = m_EntriesById.find(handle.ID); entry != m_EntriesById.end())
        {
            std::erase(references, entry->second.Path);
        }
        return references;
    }
}
