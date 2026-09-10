#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Packaging/PackageFormat.h"
#include "Utils/FileMapping.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace HachimiEngine
{
    class PackageEntryStream;

    struct PackageVerificationReport
    {
        uint32_t EntryCount = 0;
        uint32_t VerifiedCount = 0;
        uint32_t FailedCount = 0;
        std::vector<std::string> Failures;
    };

    // Reads a .hpak container without extracting it to disk. The package body is
    // served from a read-only memory mapping when possible, which makes stored
    // entries zero copy; compressed entries are inflated one block at a time.
    class PackageReader
    {
    public:
        PackageReader();
        ~PackageReader();

        PackageReader(const PackageReader&) = delete;
        PackageReader& operator=(const PackageReader&) = delete;

        bool Open(const std::filesystem::path& packagePath);
        void Close();
        bool IsOpen() const;

        const PackageFileHeader& GetHeader() const;
        uint64_t GetPackageId() const;
        uint32_t GetDictionarySize() const;
        bool IsMemoryMapped() const;

        size_t GetEntryCount() const;
        const PackageEntryInfo& GetEntry(size_t entryIndex) const;

        // Case-insensitive lookup, matching Windows path semantics.
        bool FindEntry(std::string_view virtualPath, size_t& outEntryIndex) const;
        bool HasEntry(std::string_view virtualPath) const;

        // Entry names whose directory matches: exact parent when recursive is
        // false, any depth below it when true. Results keep the stored casing.
        std::vector<std::string> EnumerateEntries(std::string_view directory, bool recursive) const;
        // True when at least one entry lives at or below the directory. Lets the
        // virtual file system report packaged directories without materializing a
        // listing.
        bool HasDirectory(std::string_view directory) const;

        bool ReadEntry(size_t entryIndex, std::vector<uint8_t>& outData) const;
        // Zero copy for stored entries backed by the memory mapping.
        bool MapEntry(size_t entryIndex, FileMapping& outMapping) const;
        // Partial read served by the entry block table.
        bool ReadEntryRange(size_t entryIndex, uint64_t offset, uint64_t size, std::vector<uint8_t>& outData) const;
        Scope<PackageEntryStream> OpenEntryStream(size_t entryIndex) const;

        // Uncompressed byte range of one block, used by streaming readers.
        bool GetBlockRange(size_t entryIndex, uint32_t blockIndex,
                           uint64_t& outUncompressedOffset, uint64_t& outUncompressedSize) const;

        bool VerifyEntry(size_t entryIndex) const;
        bool VerifyAll(PackageVerificationReport& outReport) const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
