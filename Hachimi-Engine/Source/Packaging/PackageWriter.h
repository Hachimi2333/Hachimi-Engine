#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Packaging/PackageFormat.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace HachimiEngine
{
    struct PackageWriterSettings
    {
        int CompressionLevel = static_cast<int>(PackageFormat::DefaultCompressionLevel);
        uint32_t BlockSize = PackageFormat::DefaultBlockSize;
        // Trains a shared zstd dictionary from the package contents. Falls back
        // to no dictionary when there are too few samples or training fails.
        bool UseDictionary = true;
        bool MultiThreaded = true;
    };

    struct PackageBuildReport
    {
        uint32_t EntryCount = 0;
        uint64_t UncompressedBytes = 0;
        uint64_t CompressedBytes = 0;
        uint32_t DictionarySize = 0;
        uint32_t StoredEntryCount = 0;
        uint32_t ZstdEntryCount = 0;
        uint32_t BlockCount = 0;
        uint64_t PackageId = 0;
        double Seconds = 0.0;

        double CompressionRatio() const
        {
            return UncompressedBytes == 0
                ? 0.0
                : static_cast<double>(CompressedBytes) / static_cast<double>(UncompressedBytes);
        }
    };

    // Writes a .hpak container. Entries are staged first and compressed in
    // Finalize(), so the writer can train a dictionary and compress entries in
    // parallel.
    class PackageWriter
    {
    public:
        PackageWriter();
        ~PackageWriter();

        PackageWriter(const PackageWriter&) = delete;
        PackageWriter& operator=(const PackageWriter&) = delete;

        bool Open(const std::filesystem::path& packagePath, const PackageWriterSettings& settings = {});
        bool AddFile(const std::filesystem::path& virtualPath, const std::filesystem::path& sourcePath);
        bool AddMemory(const std::filesystem::path& virtualPath, const void* data, size_t size);
        bool Finalize(PackageBuildReport& outReport);
        // Discards the staged package and removes the partially written file.
        void Abort();

        bool IsOpen() const;
        size_t GetEntryCount() const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
