#include "Packaging/PackageWriter.h"

#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Utils/FileSystem.h"
#include "Utils/XXHash.h"

#include <zdict.h>
#include <zstd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        // Already-compressed payloads gain nothing from deflate-style packing and
        // would only cost decompression time on every load.
        const char* const StoredExtensions[] =
        {
            ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif", ".dds", ".ktx", ".ktx2",
            ".ttf", ".otf", ".ttc", ".zip", ".hpak", ".ogg", ".mp3", ".wav", ".flac"
        };

        constexpr size_t DictionarySampleSizeLimit = 64u * 1024u * 1024u;
        constexpr size_t DictionaryPerFileSampleLimit = 128u * 1024u;
        constexpr size_t DictionaryMinimumSamples = 8;
        constexpr size_t DictionaryMaximumSize = 112640;
        constexpr size_t DictionaryMinimumSize = 4096;

        uint64_t AlignUp(uint64_t value, uint64_t alignment)
        {
            return ((value + alignment - 1) / alignment) * alignment;
        }

        bool ShouldStoreUncompressed(const std::string& entryName)
        {
            const std::string extension = ToLowerAscii(std::filesystem::path(entryName).extension().string());
            if (extension.empty())
            {
                return false;
            }

            for (const char* candidate : StoredExtensions)
            {
                if (extension == candidate)
                {
                    return true;
                }
            }
            return false;
        }

        template<typename T>
        void AppendPod(std::vector<uint8_t>& buffer, const T& value)
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
        }

        // One ZSTD_CCtx per thread: contexts are not shareable and allocating one
        // per entry would dominate the cost of packing many small files.
        ZSTD_CCtx* GetThreadCompressionContext()
        {
            struct ContextHolder
            {
                ZSTD_CCtx* Context = ZSTD_createCCtx();

                ~ContextHolder()
                {
                    if (Context != nullptr)
                    {
                        ZSTD_freeCCtx(Context);
                    }
                }
            };

            thread_local ContextHolder holder;
            return holder.Context;
        }

        struct StagedEntry
        {
            std::string Name;
            std::string LowerName;
            std::filesystem::path SourcePath;
            std::vector<uint8_t> OwnedData;
        };

        struct CompressedEntry
        {
            std::vector<uint8_t> BlockBytes;
            std::vector<PackageBlockRecord> Blocks;
            uint64_t UncompressedSize = 0;
            uint64_t CompressedSize = 0;
            uint64_t ContentHash = 0;
            PackageCompressionMethod Method = PackageCompressionMethod::Store;
            bool Ok = false;
        };

        // Streams the entry payload in fixed-size blocks so packing memory stays
        // bounded regardless of the largest asset.
        bool ForEachPayloadBlock(
            const StagedEntry& entry,
            uint32_t blockSize,
            const std::function<bool(const uint8_t* blockData, size_t blockSize)>& onBlock)
        {
            if (!entry.SourcePath.empty())
            {
                std::ifstream file(entry.SourcePath, std::ios::binary);
                if (!file)
                {
                    HE_CORE_ERROR("Failed to open package source file: {}", entry.SourcePath.string());
                    return false;
                }

                std::vector<uint8_t> buffer(blockSize);
                for (;;)
                {
                    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
                    const std::streamsize readCount = file.gcount();
                    if (readCount > 0 && !onBlock(buffer.data(), static_cast<size_t>(readCount)))
                    {
                        return false;
                    }

                    if (readCount < static_cast<std::streamsize>(buffer.size()))
                    {
                        // A short read is only legitimate at end of file.
                        if (!file.eof())
                        {
                            HE_CORE_ERROR("Failed to read package source file: {}", entry.SourcePath.string());
                            return false;
                        }
                        return true;
                    }
                }
            }

            const uint8_t* data = entry.OwnedData.data();
            size_t remaining = entry.OwnedData.size();
            while (remaining > 0)
            {
                const size_t chunk = std::min<size_t>(remaining, blockSize);
                if (!onBlock(data, chunk))
                {
                    return false;
                }
                data += chunk;
                remaining -= chunk;
            }
            return true;
        }

        bool CompressEntry(
            const StagedEntry& entry,
            const PackageWriterSettings& settings,
            const std::vector<uint8_t>& dictionary,
            CompressedEntry& outEntry)
        {
            outEntry = {};
            outEntry.Method = ShouldStoreUncompressed(entry.Name)
                ? PackageCompressionMethod::Store
                : PackageCompressionMethod::Zstd;

            ZSTD_CCtx* context = outEntry.Method == PackageCompressionMethod::Zstd
                ? GetThreadCompressionContext()
                : nullptr;
            if (outEntry.Method == PackageCompressionMethod::Zstd && context == nullptr)
            {
                HE_CORE_ERROR("Failed to create a zstd compression context");
                return false;
            }

            const void* dictionaryData = dictionary.empty() ? nullptr : dictionary.data();
            const size_t dictionarySize = dictionary.size();
            std::vector<uint8_t> compressedBuffer;
            XXHashStream contentHash;

            const bool readOk = ForEachPayloadBlock(entry, settings.BlockSize,
                [&](const uint8_t* blockData, size_t blockSize) -> bool
                {
                    contentHash.Update(blockData, blockSize);
                    outEntry.UncompressedSize += blockSize;

                    if (outEntry.Method == PackageCompressionMethod::Store)
                    {
                        outEntry.BlockBytes.insert(outEntry.BlockBytes.end(), blockData, blockData + blockSize);
                        outEntry.Blocks.push_back({ static_cast<uint32_t>(blockSize), static_cast<uint32_t>(blockSize) });
                        outEntry.CompressedSize += blockSize;
                        return true;
                    }

                    compressedBuffer.resize(ZSTD_compressBound(blockSize));
                    const size_t written = ZSTD_compress_usingDict(
                        context,
                        compressedBuffer.data(),
                        compressedBuffer.size(),
                        blockData,
                        blockSize,
                        dictionaryData,
                        dictionarySize,
                        settings.CompressionLevel);

                    if (ZSTD_isError(written))
                    {
                        HE_CORE_ERROR("zstd compression failed for '{}': {}", entry.Name, ZSTD_getErrorName(written));
                        return false;
                    }

                    // Invariant relied on by the reader: a zstd block is always
                    // strictly smaller than its payload, otherwise the block is
                    // stored raw with CompressedSize == UncompressedSize.
                    if (written < blockSize)
                    {
                        outEntry.BlockBytes.insert(
                            outEntry.BlockBytes.end(), compressedBuffer.data(), compressedBuffer.data() + written);
                        outEntry.Blocks.push_back(
                            { static_cast<uint32_t>(written), static_cast<uint32_t>(blockSize) });
                        outEntry.CompressedSize += written;
                    }
                    else
                    {
                        outEntry.BlockBytes.insert(outEntry.BlockBytes.end(), blockData, blockData + blockSize);
                        outEntry.Blocks.push_back(
                            { static_cast<uint32_t>(blockSize), static_cast<uint32_t>(blockSize) });
                        outEntry.CompressedSize += blockSize;
                    }
                    return true;
                });

            if (!readOk)
            {
                return false;
            }

            outEntry.ContentHash = contentHash.Digest();
            outEntry.Ok = true;
            return true;
        }

        // Trains one shared dictionary from the package contents. A trained
        // dictionary is a large win for projects made of many small text assets,
        // and the package simply falls back to no dictionary when it is not
        // worthwhile or training fails.
        std::vector<uint8_t> BuildDictionary(const std::vector<StagedEntry>& entries,
                                             const PackageWriterSettings& settings)
        {
            if (!settings.UseDictionary)
            {
                return {};
            }

            std::vector<uint8_t> samples;
            std::vector<size_t> sampleSizes;
            samples.reserve(DictionarySampleSizeLimit);

            for (const StagedEntry& entry : entries)
            {
                if (ShouldStoreUncompressed(entry.Name))
                {
                    continue;
                }

                size_t entrySize = entry.OwnedData.size();
                if (entry.SourcePath.empty() && entrySize == 0)
                {
                    continue;
                }
                if (!entry.SourcePath.empty())
                {
                    entrySize = static_cast<size_t>(FileSystem::GetFileSize(entry.SourcePath));
                }
                if (entrySize == 0)
                {
                    continue;
                }

                const size_t take = std::min(entrySize, DictionaryPerFileSampleLimit);
                if (samples.size() + take > DictionarySampleSizeLimit)
                {
                    break;
                }

                if (!entry.SourcePath.empty())
                {
                    std::ifstream file(entry.SourcePath, std::ios::binary);
                    if (!file)
                    {
                        continue;
                    }

                    const size_t begin = samples.size();
                    samples.resize(begin + take);
                    file.read(reinterpret_cast<char*>(samples.data() + begin), static_cast<std::streamsize>(take));
                    const std::streamsize readCount = file.gcount();
                    if (readCount <= 0)
                    {
                        samples.resize(begin);
                        continue;
                    }
                    samples.resize(begin + static_cast<size_t>(readCount));
                    sampleSizes.push_back(static_cast<size_t>(readCount));
                }
                else
                {
                    samples.insert(samples.end(), entry.OwnedData.begin(), entry.OwnedData.begin() + take);
                    sampleSizes.push_back(take);
                }
            }

            if (sampleSizes.size() < DictionaryMinimumSamples || samples.size() < DictionaryMinimumSize * 2)
            {
                return {};
            }

            const size_t capacity = std::clamp(
                samples.size() / 20, DictionaryMinimumSize, DictionaryMaximumSize);
            std::vector<uint8_t> dictionary(capacity);

            const size_t trained = ZDICT_trainFromBuffer(
                dictionary.data(),
                dictionary.size(),
                samples.data(),
                sampleSizes.data(),
                static_cast<unsigned int>(sampleSizes.size()));

            if (ZDICT_isError(trained))
            {
                HE_CORE_WARN("Skipping package dictionary: {}", ZDICT_getErrorName(trained));
                return {};
            }

            dictionary.resize(trained);
            return dictionary;
        }
    }

    struct PackageWriter::Impl
    {
        std::ofstream Output;
        PackageWriterSettings Settings;
        std::filesystem::path PackagePath;
        std::vector<StagedEntry> Entries;
        std::vector<uint8_t> Dictionary;
        bool IsOpen = false;
        bool IsFinalized = false;
    };

    PackageWriter::PackageWriter()
        : m_Impl(CreateScope<Impl>())
    {
    }

    PackageWriter::~PackageWriter()
    {
        Abort();
    }

    bool PackageWriter::Open(const std::filesystem::path& packagePath, const PackageWriterSettings& settings)
    {
        if (m_Impl->IsOpen)
        {
            HE_CORE_WARN("Package writer is already open");
            return false;
        }

        if (settings.BlockSize == 0)
        {
            HE_CORE_ERROR("Package writer block size must be greater than zero");
            return false;
        }

        FileSystem::CreateDirectories(packagePath.parent_path());

        m_Impl->Output.open(packagePath, std::ios::binary | std::ios::trunc);
        if (!m_Impl->Output)
        {
            HE_CORE_ERROR("Failed to create game package: {}", packagePath.string());
            return false;
        }

        m_Impl->Settings = settings;
        m_Impl->PackagePath = packagePath;
        m_Impl->Entries.clear();
        m_Impl->Dictionary.clear();
        m_Impl->IsOpen = true;
        m_Impl->IsFinalized = false;
        return true;
    }

    bool PackageWriter::AddFile(const std::filesystem::path& virtualPath, const std::filesystem::path& sourcePath)
    {
        if (!m_Impl->IsOpen || m_Impl->IsFinalized)
        {
            return false;
        }

        const std::string entryName = MakePackageEntryName(virtualPath);
        if (entryName.empty())
        {
            HE_CORE_ERROR("Refusing to package unsafe archive path: {}", virtualPath.string());
            return false;
        }

        if (!FileSystem::Exists(sourcePath))
        {
            HE_CORE_ERROR("Cannot package missing file '{}' as '{}'", sourcePath.string(), entryName);
            return false;
        }

        StagedEntry entry;
        entry.Name = entryName;
        entry.LowerName = ToLowerAscii(entryName);
        entry.SourcePath = sourcePath;
        m_Impl->Entries.push_back(std::move(entry));
        return true;
    }

    bool PackageWriter::AddMemory(const std::filesystem::path& virtualPath, const void* data, size_t size)
    {
        if (!m_Impl->IsOpen || m_Impl->IsFinalized)
        {
            return false;
        }

        if (data == nullptr && size > 0)
        {
            HE_CORE_ERROR("Cannot package null data for entry '{}'", virtualPath.string());
            return false;
        }

        const std::string entryName = MakePackageEntryName(virtualPath);
        if (entryName.empty())
        {
            HE_CORE_ERROR("Refusing to package unsafe archive path: {}", virtualPath.string());
            return false;
        }

        StagedEntry entry;
        entry.Name = entryName;
        entry.LowerName = ToLowerAscii(entryName);
        if (size > 0)
        {
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            entry.OwnedData.assign(bytes, bytes + size);
        }
        m_Impl->Entries.push_back(std::move(entry));
        return true;
    }

    bool PackageWriter::Finalize(PackageBuildReport& outReport)
    {
        outReport = {};

        if (!m_Impl->IsOpen || m_Impl->IsFinalized)
        {
            HE_CORE_ERROR("Cannot finalize a package writer that is not open");
            return false;
        }

        if (m_Impl->Entries.empty())
        {
            HE_CORE_ERROR("Cannot finalize an empty game package");
            return false;
        }

        const auto startTime = std::chrono::steady_clock::now();

        // Sorted, case-insensitive unique order makes the TOC binary searchable
        // for directory listings and keeps package output reproducible.
        std::sort(m_Impl->Entries.begin(), m_Impl->Entries.end(),
            [](const StagedEntry& lhs, const StagedEntry& rhs)
            {
                if (lhs.LowerName != rhs.LowerName)
                {
                    return lhs.LowerName < rhs.LowerName;
                }
                return lhs.Name < rhs.Name;
            });

        for (size_t index = 1; index < m_Impl->Entries.size(); ++index)
        {
            if (m_Impl->Entries[index].LowerName == m_Impl->Entries[index - 1].LowerName)
            {
                HE_CORE_ERROR("Duplicate package entry: {}", m_Impl->Entries[index].Name);
                return false;
            }
        }

        m_Impl->Dictionary = BuildDictionary(m_Impl->Entries, m_Impl->Settings);

        std::ofstream& output = m_Impl->Output;
        output.seekp(0);

        // Reserve the header; it is rewritten once the TOC offset is known.
        const std::vector<uint8_t> headerPlaceholder(sizeof(PackageFileHeader), 0);
        output.write(reinterpret_cast<const char*>(headerPlaceholder.data()),
                     static_cast<std::streamsize>(headerPlaceholder.size()));

        uint64_t offset = sizeof(PackageFileHeader);
        uint64_t dictionaryOffset = 0;
        if (!m_Impl->Dictionary.empty())
        {
            const uint64_t aligned = AlignUp(offset, PackageFormat::BlockAlignment);
            const std::vector<uint8_t> padding(static_cast<size_t>(aligned - offset), 0);
            output.write(reinterpret_cast<const char*>(padding.data()), static_cast<std::streamsize>(padding.size()));
            offset = aligned;

            dictionaryOffset = offset;
            output.write(reinterpret_cast<const char*>(m_Impl->Dictionary.data()),
                         static_cast<std::streamsize>(m_Impl->Dictionary.size()));
            offset += m_Impl->Dictionary.size();
        }

        const size_t entryCount = m_Impl->Entries.size();
        std::vector<CompressedEntry> compressed(entryCount);

        const bool parallel = m_Impl->Settings.MultiThreaded && JobSystem::IsRunning() && entryCount > 1;

        if (parallel)
        {
            std::atomic<uint32_t> remaining{ static_cast<uint32_t>(entryCount) };
            std::mutex completionMutex;
            std::condition_variable completionCondition;

            for (size_t index = 0; index < entryCount; ++index)
            {
                JobSystem::Submit([&, index]()
                {
                    const bool ok = CompressEntry(
                        m_Impl->Entries[index], m_Impl->Settings, m_Impl->Dictionary, compressed[index]);
                    compressed[index].Ok = ok;

                    {
                        std::lock_guard<std::mutex> lock(completionMutex);
                        remaining.fetch_sub(1);
                    }
                    completionCondition.notify_one();
                });
            }

            std::unique_lock<std::mutex> lock(completionMutex);
            completionCondition.wait(lock, [&remaining] { return remaining.load() == 0; });
        }
        else
        {
            for (size_t index = 0; index < entryCount; ++index)
            {
                compressed[index].Ok = CompressEntry(
                    m_Impl->Entries[index], m_Impl->Settings, m_Impl->Dictionary, compressed[index]);
            }
        }

        std::vector<PackageEntryRecord> entryRecords(entryCount);
        std::vector<PackageBlockRecord> blockRecords;
        std::string nameTable;
        uint64_t packageIdentity = 0;
        // Content identity of the package, independent of the TOC byte layout.
        XXHashStream tocContentStream;

        for (size_t index = 0; index < entryCount; ++index)
        {
            const StagedEntry& staged = m_Impl->Entries[index];
            const CompressedEntry& packed = compressed[index];

            if (!packed.Ok)
            {
                HE_CORE_ERROR("Failed to compress package entry: {}", staged.Name);
                Abort();
                return false;
            }

            const uint64_t aligned = AlignUp(offset, PackageFormat::BlockAlignment);
            const std::vector<uint8_t> padding(static_cast<size_t>(aligned - offset), 0);
            output.write(reinterpret_cast<const char*>(padding.data()), static_cast<std::streamsize>(padding.size()));
            offset = aligned;

            PackageEntryRecord& record = entryRecords[index];
            record = {};
            record.PathHash = XXHash::HashString(staged.LowerName);
            record.DataOffset = offset;
            record.UncompressedSize = packed.UncompressedSize;
            record.CompressedSize = packed.CompressedSize;
            record.ContentHash = packed.ContentHash;
            record.NameOffset = static_cast<uint32_t>(nameTable.size());
            record.NameLength = static_cast<uint32_t>(staged.Name.size());
            record.FirstBlock = static_cast<uint32_t>(blockRecords.size());
            record.BlockCount = static_cast<uint32_t>(packed.Blocks.size());
            record.Method = static_cast<uint8_t>(packed.Method);

            nameTable.append(staged.Name);
            blockRecords.insert(blockRecords.end(), packed.Blocks.begin(), packed.Blocks.end());

            tocContentStream.Update(staged.Name.data(), staged.Name.size());
            tocContentStream.Update(&record.UncompressedSize, sizeof(record.UncompressedSize));
            tocContentStream.Update(&record.ContentHash, sizeof(record.ContentHash));

            if (!packed.BlockBytes.empty())
            {
                output.write(reinterpret_cast<const char*>(packed.BlockBytes.data()),
                             static_cast<std::streamsize>(packed.BlockBytes.size()));
                offset += packed.BlockBytes.size();
            }

            outReport.UncompressedBytes += packed.UncompressedSize;
            outReport.CompressedBytes += packed.CompressedSize;
            outReport.BlockCount += static_cast<uint32_t>(packed.Blocks.size());
            if (packed.Method == PackageCompressionMethod::Store)
            {
                ++outReport.StoredEntryCount;
            }
            else
            {
                ++outReport.ZstdEntryCount;
            }

            packageIdentity = XXHash::HashString(
                std::to_string(packageIdentity) + staged.Name + std::to_string(packed.ContentHash));
        }

        // Table of contents: header + entry records + block records + names.
        std::vector<uint8_t> toc;
        toc.reserve(sizeof(PackageTocHeader) + entryRecords.size() * sizeof(PackageEntryRecord)
            + blockRecords.size() * sizeof(PackageBlockRecord) + nameTable.size());

        PackageTocHeader tocHeader = {};
        tocHeader.EntryCount = static_cast<uint32_t>(entryRecords.size());
        tocHeader.BlockCount = static_cast<uint32_t>(blockRecords.size());
        tocHeader.NameTableSize = nameTable.size();
        tocHeader.TocContentHash = tocContentStream.Digest();
        AppendPod(toc, tocHeader);

        for (const PackageEntryRecord& record : entryRecords)
        {
            AppendPod(toc, record);
        }
        for (const PackageBlockRecord& record : blockRecords)
        {
            AppendPod(toc, record);
        }
        toc.insert(toc.end(), nameTable.begin(), nameTable.end());

        // Covers every TOC byte except the content hash field, which the reader
        // recomputes the same way.
        const uint64_t tocHash = ComputePackageTocHash(toc.data(), toc.size());

        const uint64_t tocOffset = offset;
        output.write(reinterpret_cast<const char*>(toc.data()), static_cast<std::streamsize>(toc.size()));

        PackageFooter footer = {};
        std::memcpy(footer.Magic, PackageFormat::FooterMagic, sizeof(PackageFormat::FooterMagic));
        footer.TocOffset = tocOffset;
        footer.TocSize = toc.size();
        footer.TocHash = tocHash;
        output.write(reinterpret_cast<const char*>(&footer), static_cast<std::streamsize>(sizeof(footer)));

        PackageFileHeader header = {};
        std::memcpy(header.Magic, PackageFormat::Magic, sizeof(PackageFormat::Magic));
        header.FormatVersion = PackageFormat::Version;
        header.HeaderSize = sizeof(PackageFileHeader);
        header.PackageId = packageIdentity;
        header.TocOffset = tocOffset;
        header.TocSize = toc.size();
        header.DictionaryOffset = dictionaryOffset;
        header.DictionarySize = static_cast<uint32_t>(m_Impl->Dictionary.size());
        header.CompressionLevel = static_cast<uint32_t>(m_Impl->Settings.CompressionLevel);
        header.BlockSize = m_Impl->Settings.BlockSize;
        header.Flags = static_cast<uint32_t>(m_Impl->Dictionary.empty()
            ? PackageFlags::None
            : PackageFlags::HasDictionary);

        output.seekp(0);
        output.write(reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));
        output.flush();

        if (!output)
        {
            HE_CORE_ERROR("Failed to write game package: {}", m_Impl->PackagePath.string());
            Abort();
            return false;
        }

        output.close();
        m_Impl->IsFinalized = true;
        m_Impl->IsOpen = false;

        outReport.EntryCount = static_cast<uint32_t>(entryCount);
        outReport.DictionarySize = static_cast<uint32_t>(m_Impl->Dictionary.size());
        outReport.PackageId = packageIdentity;
        outReport.Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
        return true;
    }

    void PackageWriter::Abort()
    {
        if (m_Impl->Output.is_open())
        {
            m_Impl->Output.close();
        }

        if (m_Impl->IsOpen && !m_Impl->IsFinalized && !m_Impl->PackagePath.empty())
        {
            FileSystem::RemoveAll(m_Impl->PackagePath);
        }

        m_Impl->IsOpen = false;
        m_Impl->Entries.clear();
        m_Impl->Dictionary.clear();
    }

    bool PackageWriter::IsOpen() const
    {
        return m_Impl->IsOpen;
    }

    size_t PackageWriter::GetEntryCount() const
    {
        return m_Impl->Entries.size();
    }
}
