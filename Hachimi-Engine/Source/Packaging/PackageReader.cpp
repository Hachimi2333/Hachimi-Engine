#include "Packaging/PackageReader.h"

#include "Core/Log.h"
#include "Packaging/PackageEntryStream.h"
#include "Utils/FileSystem.h"
#include "Utils/XXHash.h"

#include <zstd.h>

#ifdef HE_PLATFORM_WINDOWS
// windows.h defines min/max as macros, which breaks std::min/std::max below.
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        ZSTD_DCtx* GetThreadDecompressionContext()
        {
            struct ContextHolder
            {
                ZSTD_DCtx* Context = ZSTD_createDCtx();

                ~ContextHolder()
                {
                    if (Context != nullptr)
                    {
                        ZSTD_freeDCtx(Context);
                    }
                }
            };

            thread_local ContextHolder holder;
            return holder.Context;
        }

        struct BlockView
        {
            const uint8_t* Data = nullptr;
            size_t Size = 0;
        };
    }

    struct PackageReader::Impl
    {
        std::filesystem::path PackagePath;
        // Reading mutates the stream position, so the fallback reader has to stay
        // usable from const read paths.
        mutable std::ifstream Stream;

#ifdef HE_PLATFORM_WINDOWS
        HANDLE FileHandle = INVALID_HANDLE_VALUE;
        HANDLE MappingHandle = nullptr;
#endif

        const uint8_t* MappedData = nullptr;
        uint64_t FileSize = 0;
        bool IsOpen = false;

        PackageFileHeader Header = {};
        std::vector<PackageEntryInfo> Entries;
        std::vector<PackageBlockRecord> Blocks;
        // Absolute file offsets and uncompressed offsets per block, precomputed so
        // random reads never rescan the entry.
        std::vector<uint64_t> BlockDataOffsets;
        std::vector<uint64_t> BlockUncompressedOffsets;
        std::vector<std::string> LowerNames;
        // Sorted by LowerNames for directory enumeration.
        std::vector<size_t> SortedIndices;
        // Keys point into LowerNames, which is never resized after Open().
        std::unordered_map<std::string_view, size_t> Lookup;
        std::vector<uint8_t> Dictionary;

        void CloseHandles()
        {
#ifdef HE_PLATFORM_WINDOWS
            if (MappedData != nullptr)
            {
                UnmapViewOfFile(MappedData);
                MappedData = nullptr;
            }
            if (MappingHandle != nullptr)
            {
                CloseHandle(MappingHandle);
                MappingHandle = nullptr;
            }
            if (FileHandle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(FileHandle);
                FileHandle = INVALID_HANDLE_VALUE;
            }
#endif
            if (Stream.is_open())
            {
                Stream.close();
            }
            MappedData = nullptr;
            FileSize = 0;
        }

        bool ReadAt(uint64_t offset, void* destination, size_t size) const
        {
            if (size == 0)
            {
                return true;
            }

            if (offset > FileSize || size > FileSize - offset)
            {
                return false;
            }

            if (MappedData != nullptr)
            {
                std::memcpy(destination, MappedData + offset, size);
                return true;
            }

            if (!Stream.is_open())
            {
                return false;
            }

            Stream.clear();
            Stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            if (!Stream)
            {
                return false;
            }

            Stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
            return Stream.gcount() == static_cast<std::streamsize>(size);
        }

        // Reads raw block bytes, either as a pointer into the mapping or into the
        // supplied scratch buffer.
        bool AcquireRawBlock(const PackageEntryInfo& entry, uint32_t localBlockIndex,
                             std::vector<uint8_t>& scratch, BlockView& outView) const
        {
            const size_t globalBlock = entry.FirstBlock + localBlockIndex;
            if (globalBlock >= Blocks.size())
            {
                return false;
            }

            const PackageBlockRecord& record = Blocks[globalBlock];
            const uint64_t blockOffset = BlockDataOffsets[globalBlock];

            if (record.CompressedSize == 0 && record.UncompressedSize == 0)
            {
                outView = {};
                return true;
            }

            if (MappedData != nullptr)
            {
                if (blockOffset > FileSize || record.CompressedSize > FileSize - blockOffset)
                {
                    return false;
                }
                outView.Data = MappedData + blockOffset;
                outView.Size = record.CompressedSize;
                return true;
            }

            scratch.resize(record.CompressedSize);
            if (!ReadAt(blockOffset, scratch.data(), scratch.size()))
            {
                HE_CORE_ERROR("Failed to read package block data for entry: {}", entry.VirtualPath.string());
                return false;
            }
            outView.Data = scratch.data();
            outView.Size = scratch.size();
            return true;
        }

        // Returns the uncompressed bytes of one block.
        bool AcquireBlock(const PackageEntryInfo& entry, uint32_t localBlockIndex,
                          std::vector<uint8_t>& scratch, BlockView& outView) const
        {
            BlockView raw;
            if (!AcquireRawBlock(entry, localBlockIndex, scratch, raw))
            {
                return false;
            }

            const size_t globalBlock = entry.FirstBlock + localBlockIndex;
            const PackageBlockRecord& record = Blocks[globalBlock];

            if (record.UncompressedSize == 0)
            {
                outView = {};
                return true;
            }

            // A stored block keeps CompressedSize == UncompressedSize; the writer
            // guarantees a zstd block is always strictly smaller than its payload.
            if (record.CompressedSize == record.UncompressedSize)
            {
                outView = raw;
                return true;
            }

            // Decompress into a fresh buffer; scratch keeps the compressed bytes
            // alive while zstd reads them.
            std::vector<uint8_t> decompressed(record.UncompressedSize);
            ZSTD_DCtx* context = GetThreadDecompressionContext();
            if (context == nullptr)
            {
                HE_CORE_ERROR("Failed to create a zstd decompression context");
                return false;
            }

            const void* dictionaryData = Dictionary.empty() ? nullptr : Dictionary.data();
            const size_t result = ZSTD_decompress_usingDict(
                context,
                decompressed.data(),
                decompressed.size(),
                raw.Data,
                raw.Size,
                dictionaryData,
                Dictionary.size());

            if (ZSTD_isError(result) || result != record.UncompressedSize)
            {
                HE_CORE_ERROR("Failed to decompress package entry '{}': {}",
                    entry.VirtualPath.string(),
                    ZSTD_isError(result) ? ZSTD_getErrorName(result) : "size mismatch");
                return false;
            }

            scratch = std::move(decompressed);
            outView.Data = scratch.data();
            outView.Size = scratch.size();
            return true;
        }
    };

    PackageReader::PackageReader()
        : m_Impl(CreateScope<Impl>())
    {
    }

    PackageReader::~PackageReader()
    {
        Close();
    }

    void PackageReader::Close()
    {
        m_Impl->CloseHandles();
        m_Impl->IsOpen = false;
        m_Impl->Header = {};
        m_Impl->Entries.clear();
        m_Impl->Blocks.clear();
        m_Impl->BlockDataOffsets.clear();
        m_Impl->BlockUncompressedOffsets.clear();
        m_Impl->LowerNames.clear();
        m_Impl->SortedIndices.clear();
        m_Impl->Lookup.clear();
        m_Impl->Dictionary.clear();
    }

    bool PackageReader::Open(const std::filesystem::path& packagePath)
    {
        Close();

        m_Impl->FileSize = FileSystem::GetFileSize(packagePath);
        if (m_Impl->FileSize == 0)
        {
            HE_CORE_ERROR("Game package is missing or empty: {}", packagePath.string());
            return false;
        }

        m_Impl->PackagePath = packagePath;

#ifdef HE_PLATFORM_WINDOWS
        // A read-only mapping turns stored entries into zero-copy views and lets
        // the OS page cache serve repeated block reads.
        m_Impl->FileHandle = CreateFileW(
            packagePath.wstring().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (m_Impl->FileHandle != INVALID_HANDLE_VALUE)
        {
            LARGE_INTEGER size = {};
            if (GetFileSizeEx(m_Impl->FileHandle, &size) && size.QuadPart > 0)
            {
                m_Impl->MappingHandle = CreateFileMappingW(
                    m_Impl->FileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
                if (m_Impl->MappingHandle != nullptr)
                {
                    m_Impl->MappedData = static_cast<const uint8_t*>(
                        MapViewOfFile(m_Impl->MappingHandle, FILE_MAP_READ, 0, 0, 0));
                }
            }
        }
#endif

        if (m_Impl->MappedData == nullptr)
        {
            m_Impl->Stream.open(packagePath, std::ios::binary);
            if (!m_Impl->Stream)
            {
                HE_CORE_ERROR("Failed to open game package: {}", packagePath.string());
                m_Impl->CloseHandles();
                return false;
            }
        }

        // Header.
        PackageFileHeader header = {};
        if (!m_Impl->ReadAt(0, &header, sizeof(header)))
        {
            HE_CORE_ERROR("Failed to read the game package header: {}", packagePath.string());
            Close();
            return false;
        }

        if (std::memcmp(header.Magic, PackageFormat::Magic, sizeof(PackageFormat::Magic)) != 0)
        {
            HE_CORE_ERROR("'{}' is not a Hachimi game package", packagePath.string());
            Close();
            return false;
        }

        if (header.FormatVersion != PackageFormat::Version)
        {
            HE_CORE_ERROR("Unsupported game package version {} (expected {}): {}",
                header.FormatVersion, PackageFormat::Version, packagePath.string());
            Close();
            return false;
        }

        if (header.HeaderSize != sizeof(PackageFileHeader))
        {
            HE_CORE_ERROR("Game package header size mismatch: {}", packagePath.string());
            Close();
            return false;
        }

        if (header.TocOffset > m_Impl->FileSize || header.TocSize > m_Impl->FileSize - header.TocOffset
            || header.TocSize < sizeof(PackageTocHeader))
        {
            HE_CORE_ERROR("Game package table of contents is out of range: {}", packagePath.string());
            Close();
            return false;
        }

        // Footer cross-check: the TOC must be reachable from both ends.
        PackageFooter footer = {};
        if (!m_Impl->ReadAt(m_Impl->FileSize - sizeof(footer), &footer, sizeof(footer)))
        {
            HE_CORE_ERROR("Failed to read the game package footer: {}", packagePath.string());
            Close();
            return false;
        }

        if (std::memcmp(footer.Magic, PackageFormat::FooterMagic, sizeof(PackageFormat::FooterMagic)) != 0
            || footer.TocOffset != header.TocOffset
            || footer.TocSize != header.TocSize)
        {
            HE_CORE_ERROR("Game package footer does not match its header: {}", packagePath.string());
            Close();
            return false;
        }

        // Table of contents.
        std::vector<uint8_t> toc(static_cast<size_t>(header.TocSize));
        if (!m_Impl->ReadAt(header.TocOffset, toc.data(), toc.size()))
        {
            HE_CORE_ERROR("Failed to read the game package table of contents: {}", packagePath.string());
            Close();
            return false;
        }

        if (ComputePackageTocHash(toc.data(), toc.size()) != footer.TocHash)
        {
            HE_CORE_ERROR("Game package table of contents is corrupted: {}", packagePath.string());
            Close();
            return false;
        }

        PackageTocHeader tocHeader = {};
        std::memcpy(&tocHeader, toc.data(), sizeof(tocHeader));

        const uint64_t expectedTocSize = sizeof(PackageTocHeader)
            + static_cast<uint64_t>(tocHeader.EntryCount) * sizeof(PackageEntryRecord)
            + static_cast<uint64_t>(tocHeader.BlockCount) * sizeof(PackageBlockRecord)
            + tocHeader.NameTableSize;

        if (expectedTocSize != toc.size())
        {
            HE_CORE_ERROR("Game package table of contents has an unexpected size: {}", packagePath.string());
            Close();
            return false;
        }

        const uint8_t* entryRecords = toc.data() + sizeof(PackageTocHeader);
        const uint8_t* blockRecords = entryRecords + static_cast<size_t>(tocHeader.EntryCount) * sizeof(PackageEntryRecord);
        const char* nameTable = reinterpret_cast<const char*>(blockRecords
            + static_cast<size_t>(tocHeader.BlockCount) * sizeof(PackageBlockRecord));

        m_Impl->Blocks.resize(tocHeader.BlockCount);
        if (tocHeader.BlockCount > 0)
        {
            std::memcpy(m_Impl->Blocks.data(), blockRecords,
                static_cast<size_t>(tocHeader.BlockCount) * sizeof(PackageBlockRecord));
        }

        m_Impl->Entries.reserve(tocHeader.EntryCount);
        m_Impl->LowerNames.reserve(tocHeader.EntryCount);

        bool valid = true;
        for (uint32_t index = 0; index < tocHeader.EntryCount && valid; ++index)
        {
            PackageEntryRecord record = {};
            std::memcpy(&record, entryRecords + static_cast<size_t>(index) * sizeof(PackageEntryRecord),
                sizeof(record));

            if (record.NameOffset > tocHeader.NameTableSize
                || record.NameLength > tocHeader.NameTableSize - record.NameOffset
                || record.NameLength == 0)
            {
                valid = false;
                break;
            }

            const std::string name(nameTable + record.NameOffset, record.NameLength);
            if (!IsSafePackageEntryName(name))
            {
                HE_CORE_WARN("Ignoring unsafe package entry name: {}", name);
                valid = false;
                break;
            }

            if (record.Method != static_cast<uint8_t>(PackageCompressionMethod::Store)
                && record.Method != static_cast<uint8_t>(PackageCompressionMethod::Zstd))
            {
                valid = false;
                break;
            }

            const std::string lowerName = ToLowerAscii(name);
            if (record.PathHash != XXHash::HashString(lowerName))
            {
                HE_CORE_ERROR("Package entry '{}' failed its path hash check", name);
                valid = false;
                break;
            }

            if (record.FirstBlock > tocHeader.BlockCount
                || record.BlockCount > tocHeader.BlockCount - record.FirstBlock)
            {
                valid = false;
                break;
            }

            PackageEntryInfo entry;
            entry.VirtualPath = std::filesystem::path(name);
            entry.DataOffset = record.DataOffset;
            entry.UncompressedSize = record.UncompressedSize;
            entry.CompressedSize = record.CompressedSize;
            entry.ContentHash = record.ContentHash;
            entry.FirstBlock = record.FirstBlock;
            entry.BlockCount = record.BlockCount;
            entry.Method = static_cast<PackageCompressionMethod>(record.Method);
            m_Impl->Entries.push_back(std::move(entry));
            m_Impl->LowerNames.push_back(lowerName);
        }

        if (!valid)
        {
            HE_CORE_ERROR("Game package table of contents is malformed: {}", packagePath.string());
            Close();
            return false;
        }

        // Derive per-block offsets and validate each entry against its blocks.
        m_Impl->BlockDataOffsets.assign(m_Impl->Blocks.size(), 0);
        m_Impl->BlockUncompressedOffsets.assign(m_Impl->Blocks.size(), 0);

        for (const PackageEntryInfo& entry : m_Impl->Entries)
        {
            uint64_t dataOffset = entry.DataOffset;
            uint64_t uncompressedOffset = 0;

            for (uint32_t local = 0; local < entry.BlockCount; ++local)
            {
                const size_t global = entry.FirstBlock + local;
                const PackageBlockRecord& record = m_Impl->Blocks[global];

                if (record.CompressedSize == 0 && record.UncompressedSize != 0)
                {
                    valid = false;
                    break;
                }

                m_Impl->BlockDataOffsets[global] = dataOffset;
                m_Impl->BlockUncompressedOffsets[global] = uncompressedOffset;

                dataOffset += record.CompressedSize;
                uncompressedOffset += record.UncompressedSize;
            }

            if (!valid
                || uncompressedOffset != entry.UncompressedSize
                || dataOffset - entry.DataOffset != entry.CompressedSize
                || dataOffset > m_Impl->FileSize)
            {
                HE_CORE_ERROR("Package entry '{}' is inconsistent with its block table", entry.VirtualPath.string());
                Close();
                return false;
            }
        }

        if (HasFlag(static_cast<PackageFlags>(header.Flags), PackageFlags::HasDictionary)
            && header.DictionarySize > 0)
        {
            if (header.DictionaryOffset > m_Impl->FileSize
                || header.DictionarySize > m_Impl->FileSize - header.DictionaryOffset)
            {
                HE_CORE_ERROR("Game package dictionary is out of range: {}", packagePath.string());
                Close();
                return false;
            }

            m_Impl->Dictionary.resize(header.DictionarySize);
            if (!m_Impl->ReadAt(header.DictionaryOffset, m_Impl->Dictionary.data(), m_Impl->Dictionary.size()))
            {
                HE_CORE_ERROR("Failed to read the game package dictionary: {}", packagePath.string());
                Close();
                return false;
            }
        }

        // Lookup keys point into LowerNames, which is complete and never resized
        // again for the lifetime of this reader.
        m_Impl->Lookup.reserve(m_Impl->LowerNames.size());
        m_Impl->SortedIndices.resize(m_Impl->LowerNames.size());
        for (size_t index = 0; index < m_Impl->LowerNames.size(); ++index)
        {
            m_Impl->Lookup.emplace(std::string_view(m_Impl->LowerNames[index]), index);
            m_Impl->SortedIndices[index] = index;
        }

        std::sort(m_Impl->SortedIndices.begin(), m_Impl->SortedIndices.end(),
            [this](size_t lhs, size_t rhs)
            {
                return m_Impl->LowerNames[lhs] < m_Impl->LowerNames[rhs];
            });

        m_Impl->Header = header;
        m_Impl->IsOpen = true;
        return true;
    }

    bool PackageReader::IsOpen() const
    {
        return m_Impl->IsOpen;
    }

    const PackageFileHeader& PackageReader::GetHeader() const
    {
        return m_Impl->Header;
    }

    uint64_t PackageReader::GetPackageId() const
    {
        return m_Impl->Header.PackageId;
    }

    uint32_t PackageReader::GetDictionarySize() const
    {
        return static_cast<uint32_t>(m_Impl->Dictionary.size());
    }

    bool PackageReader::IsMemoryMapped() const
    {
        return m_Impl->MappedData != nullptr;
    }

    size_t PackageReader::GetEntryCount() const
    {
        return m_Impl->Entries.size();
    }

    const PackageEntryInfo& PackageReader::GetEntry(size_t entryIndex) const
    {
        return m_Impl->Entries[entryIndex];
    }

    bool PackageReader::FindEntry(std::string_view virtualPath, size_t& outEntryIndex) const
    {
        if (!m_Impl->IsOpen)
        {
            return false;
        }

        const std::string entryName = MakePackageEntryName(std::filesystem::path(virtualPath));
        if (entryName.empty())
        {
            return false;
        }

        const auto found = m_Impl->Lookup.find(ToLowerAscii(entryName));
        if (found == m_Impl->Lookup.end())
        {
            return false;
        }

        outEntryIndex = found->second;
        return true;
    }

    bool PackageReader::HasEntry(std::string_view virtualPath) const
    {
        size_t index = 0;
        return FindEntry(virtualPath, index);
    }

    std::vector<std::string> PackageReader::EnumerateEntries(std::string_view directory, bool recursive) const
    {
        std::vector<std::string> result;
        if (!m_Impl->IsOpen)
        {
            return result;
        }

        std::string prefix = ToLowerAscii(directory);
        while (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\'))
        {
            prefix.pop_back();
        }
        while (prefix.starts_with("./"))
        {
            prefix.erase(0, 2);
        }

        const std::string searchPrefix = prefix.empty() ? std::string() : prefix + "/";

        // Entries are indexed by lowercased name, so every candidate forms one
        // contiguous range.
        const auto begin = std::lower_bound(
            m_Impl->SortedIndices.begin(), m_Impl->SortedIndices.end(), searchPrefix,
            [this](size_t index, const std::string& key)
            {
                return m_Impl->LowerNames[index] < key;
            });

        for (auto it = begin; it != m_Impl->SortedIndices.end(); ++it)
        {
            const std::string& lowerName = m_Impl->LowerNames[*it];
            if (!lowerName.starts_with(searchPrefix))
            {
                break;
            }

            if (!recursive && GetPackageEntryDirectory(lowerName) != prefix)
            {
                continue;
            }

            result.push_back(m_Impl->Entries[*it].VirtualPath.generic_string());
        }

        return result;
    }

    bool PackageReader::HasDirectory(std::string_view directory) const
    {
        if (!m_Impl->IsOpen || m_Impl->Entries.empty())
        {
            return false;
        }

        std::string prefix = ToLowerAscii(directory);
        while (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\'))
        {
            prefix.pop_back();
        }
        while (prefix.starts_with("./"))
        {
            prefix.erase(0, 2);
        }

        if (prefix.empty())
        {
            return true;
        }

        const std::string searchPrefix = prefix + "/";
        const auto found = std::lower_bound(
            m_Impl->SortedIndices.begin(), m_Impl->SortedIndices.end(), searchPrefix,
            [this](size_t index, const std::string& key)
            {
                return m_Impl->LowerNames[index] < key;
            });

        return found != m_Impl->SortedIndices.end()
            && m_Impl->LowerNames[*found].starts_with(searchPrefix);
    }

    bool PackageReader::ReadEntry(size_t entryIndex, std::vector<uint8_t>& outData) const
    {
        outData.clear();

        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return false;
        }

        const PackageEntryInfo& entry = m_Impl->Entries[entryIndex];
        if (entry.UncompressedSize == 0)
        {
            return true;
        }

        // Stored entries served straight from the mapping need no copy at all;
        // MapEntry is the zero-copy path, this one always produces owned bytes.
        outData.resize(static_cast<size_t>(entry.UncompressedSize));

        if (entry.Method == PackageCompressionMethod::Store && m_Impl->MappedData != nullptr)
        {
            if (entry.DataOffset > m_Impl->FileSize || entry.CompressedSize > m_Impl->FileSize - entry.DataOffset)
            {
                outData.clear();
                return false;
            }
            std::memcpy(outData.data(), m_Impl->MappedData + entry.DataOffset, outData.size());
            return true;
        }

        std::vector<uint8_t> scratch;
        uint64_t written = 0;
        for (uint32_t local = 0; local < entry.BlockCount; ++local)
        {
            BlockView view;
            if (!m_Impl->AcquireBlock(entry, local, scratch, view))
            {
                outData.clear();
                return false;
            }

            if (view.Size > outData.size() - static_cast<size_t>(written))
            {
                outData.clear();
                return false;
            }

            std::memcpy(outData.data() + written, view.Data, view.Size);
            written += view.Size;
        }

        if (written != entry.UncompressedSize)
        {
            HE_CORE_ERROR("Package entry '{}' decompressed to an unexpected size", entry.VirtualPath.string());
            outData.clear();
            return false;
        }

        return true;
    }

    bool PackageReader::MapEntry(size_t entryIndex, FileMapping& outMapping) const
    {
        outMapping.Reset();

        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return false;
        }

        const PackageEntryInfo& entry = m_Impl->Entries[entryIndex];

        if (entry.Method == PackageCompressionMethod::Store && m_Impl->MappedData != nullptr)
        {
            if (entry.UncompressedSize > 0
                && (entry.DataOffset > m_Impl->FileSize
                    || entry.CompressedSize > m_Impl->FileSize - entry.DataOffset))
            {
                return false;
            }

            outMapping = FileMapping::Borrow(m_Impl->MappedData + entry.DataOffset,
                static_cast<size_t>(entry.UncompressedSize));
            return true;
        }

        std::vector<uint8_t> data;
        if (!ReadEntry(entryIndex, data))
        {
            return false;
        }

        outMapping = FileMapping::Own(std::move(data));
        return true;
    }

    bool PackageReader::ReadEntryRange(size_t entryIndex, uint64_t offset, uint64_t size,
                                       std::vector<uint8_t>& outData) const
    {
        outData.clear();

        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return false;
        }

        const PackageEntryInfo& entry = m_Impl->Entries[entryIndex];
        if (offset > entry.UncompressedSize)
        {
            return false;
        }

        size = std::min(size, entry.UncompressedSize - offset);
        if (size == 0)
        {
            return true;
        }

        // Locate the block range covering [offset, offset + size).
        uint32_t firstBlock = 0;
        uint32_t lastBlock = entry.BlockCount > 0 ? entry.BlockCount - 1 : 0;
        for (uint32_t local = 0; local < entry.BlockCount; ++local)
        {
            const size_t global = entry.FirstBlock + local;
            const uint64_t blockBegin = m_Impl->BlockUncompressedOffsets[global];
            const uint64_t blockEnd = blockBegin + m_Impl->Blocks[global].UncompressedSize;

            if (blockEnd > offset)
            {
                firstBlock = local;
                break;
            }
        }
        for (uint32_t local = firstBlock; local < entry.BlockCount; ++local)
        {
            const size_t global = entry.FirstBlock + local;
            const uint64_t blockBegin = m_Impl->BlockUncompressedOffsets[global];
            const uint64_t blockEnd = blockBegin + m_Impl->Blocks[global].UncompressedSize;
            lastBlock = local;
            if (blockEnd >= offset + size)
            {
                break;
            }
        }

        outData.resize(static_cast<size_t>(size));

        const uint64_t requestEnd = offset + size;
        std::vector<uint8_t> scratch;
        for (uint32_t local = firstBlock; local <= lastBlock; ++local)
        {
            BlockView view;
            if (!m_Impl->AcquireBlock(entry, local, scratch, view))
            {
                outData.clear();
                return false;
            }

            const size_t global = entry.FirstBlock + local;
            const uint64_t blockBegin = m_Impl->BlockUncompressedOffsets[global];
            const uint64_t blockEnd = blockBegin + view.Size;

            const uint64_t copyBegin = std::max(blockBegin, offset);
            const uint64_t copyEnd = std::min(blockEnd, requestEnd);
            if (copyEnd <= copyBegin)
            {
                continue;
            }

            std::memcpy(
                outData.data() + (copyBegin - offset),
                view.Data + (copyBegin - blockBegin),
                static_cast<size_t>(copyEnd - copyBegin));
        }

        return true;
    }

    Scope<PackageEntryStream> PackageReader::OpenEntryStream(size_t entryIndex) const
    {
        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return nullptr;
        }
        return Scope<PackageEntryStream>(new PackageEntryStream(*this, entryIndex));
    }

    bool PackageReader::GetBlockRange(size_t entryIndex, uint32_t blockIndex,
                                      uint64_t& outUncompressedOffset, uint64_t& outUncompressedSize) const
    {
        outUncompressedOffset = 0;
        outUncompressedSize = 0;

        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return false;
        }

        const PackageEntryInfo& entry = m_Impl->Entries[entryIndex];
        if (blockIndex >= entry.BlockCount)
        {
            return false;
        }

        const size_t global = entry.FirstBlock + blockIndex;
        outUncompressedOffset = m_Impl->BlockUncompressedOffsets[global];
        outUncompressedSize = m_Impl->Blocks[global].UncompressedSize;
        return true;
    }

    bool PackageReader::VerifyEntry(size_t entryIndex) const
    {
        if (!m_Impl->IsOpen || entryIndex >= m_Impl->Entries.size())
        {
            return false;
        }

        const PackageEntryInfo& entry = m_Impl->Entries[entryIndex];
        if (entry.UncompressedSize == 0)
        {
            // The writer hashes an empty payload the same way.
            return entry.ContentHash == XXHash::HashString("");
        }

        // Hashed block by block so verification never materializes the entry.
        XXHashStream hash;
        std::vector<uint8_t> scratch;
        for (uint32_t local = 0; local < entry.BlockCount; ++local)
        {
            BlockView view;
            if (!m_Impl->AcquireBlock(entry, local, scratch, view))
            {
                return false;
            }
            hash.Update(view.Data, view.Size);
        }

        return hash.Digest() == entry.ContentHash;
    }

    bool PackageReader::VerifyAll(PackageVerificationReport& outReport) const
    {
        outReport = {};
        if (!m_Impl->IsOpen)
        {
            return false;
        }

        outReport.EntryCount = static_cast<uint32_t>(m_Impl->Entries.size());
        for (size_t index = 0; index < m_Impl->Entries.size(); ++index)
        {
            if (VerifyEntry(index))
            {
                ++outReport.VerifiedCount;
            }
            else
            {
                ++outReport.FailedCount;
                outReport.Failures.push_back(m_Impl->Entries[index].VirtualPath.generic_string());
            }
        }

        return outReport.FailedCount == 0;
    }
}
