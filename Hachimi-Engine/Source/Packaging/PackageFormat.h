#pragma once

#include "Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace HachimiEngine
{
    // On-disk .hpak container layout.
    //
    // All integers are little-endian and the engine targets x86_64 Windows only,
    // so no byte swapping is performed. A package is laid out as:
    //
    //   [PackageFileHeader]  @0
    //   [dictionary]         optional raw zstd dictionary
    //   [block data]         entries are contiguous, blocks 16-byte aligned
    //   [table of contents]  entries + block records + name table
    //   [PackageFooter]      @EOF, mirrors the TOC location so a reader can
    //                        locate the directory from the end of the file
    namespace PackageFormat
    {
        inline constexpr uint8_t Magic[8] = { 'H', 'E', 'P', 'A', 'K', '2', '\r', '\n' };
        inline constexpr uint8_t FooterMagic[8] = { 'H', 'E', 'P', 'A', 'K', 'E', 'N', 'D' };
        inline constexpr uint32_t Version = 2;
        inline constexpr uint32_t DefaultBlockSize = 256u * 1024u;
        inline constexpr uint64_t BlockAlignment = 16;
        inline constexpr uint32_t DefaultCompressionLevel = 3;
    }

    // How an entry (or one of its blocks) is stored.
    enum class PackageCompressionMethod : uint8_t
    {
        Store = 0,
        Zstd = 1
    };

    enum class PackageFlags : uint32_t
    {
        None = 0,
        HasDictionary = 1u << 0
    };

    constexpr PackageFlags operator|(PackageFlags lhs, PackageFlags rhs)
    {
        return static_cast<PackageFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr bool HasFlag(PackageFlags flags, PackageFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }

#pragma pack(push, 1)

    struct PackageFileHeader
    {
        uint8_t Magic[8];
        uint32_t FormatVersion;
        uint32_t HeaderSize;
        uint64_t PackageId;
        uint64_t TocOffset;
        uint64_t TocSize;
        uint64_t DictionaryOffset;
        uint32_t DictionarySize;
        uint32_t CompressionLevel;
        uint32_t BlockSize;
        uint32_t Flags;
        uint8_t Reserved[8];
    };

    struct PackageTocHeader
    {
        uint32_t EntryCount;
        uint32_t BlockCount;
        uint64_t NameTableSize;
        uint64_t TocContentHash;
    };

    struct PackageEntryRecord
    {
        uint64_t PathHash;
        uint64_t DataOffset;
        uint64_t UncompressedSize;
        uint64_t CompressedSize;
        uint64_t ContentHash;
        uint32_t NameOffset;
        uint32_t NameLength;
        uint32_t FirstBlock;
        uint32_t BlockCount;
        uint8_t Method;
        uint8_t Reserved[7];
    };

    struct PackageBlockRecord
    {
        uint32_t CompressedSize;
        uint32_t UncompressedSize;
    };

    struct PackageFooter
    {
        uint8_t Magic[8];
        uint64_t TocOffset;
        uint64_t TocSize;
        uint64_t TocHash;
    };

#pragma pack(pop)

    static_assert(sizeof(PackageFileHeader) == 72, "PackageFileHeader layout changed");
    static_assert(sizeof(PackageTocHeader) == 24, "PackageTocHeader layout changed");
    static_assert(sizeof(PackageEntryRecord) == 64, "PackageEntryRecord layout changed");
    static_assert(sizeof(PackageBlockRecord) == 8, "PackageBlockRecord layout changed");
    static_assert(sizeof(PackageFooter) == 32, "PackageFooter layout changed");

    // Runtime view of one packaged file, built from the table of contents.
    struct PackageEntryInfo
    {
        std::filesystem::path VirtualPath;
        uint64_t DataOffset = 0;
        uint64_t UncompressedSize = 0;
        uint64_t CompressedSize = 0;
        uint64_t ContentHash = 0;
        uint32_t FirstBlock = 0;
        uint32_t BlockCount = 0;
        PackageCompressionMethod Method = PackageCompressionMethod::Store;
    };

    // Settings embedded in a packaged game, read by the Player before it creates
    // the runtime window.
    struct PackageBuildInfo
    {
        std::string ProductName;
        std::filesystem::path StartScene;
        uint32_t WindowWidth = 1600;
        uint32_t WindowHeight = 900;
        bool VSync = true;

        bool IsValid() const { return !ProductName.empty() && !StartScene.empty(); }
    };

    // Parses BuildInfo.yaml content that lives inside a package.
    bool ParsePackageBuildInfo(const std::string& yamlText, PackageBuildInfo& outBuildInfo);

    // Hashes a serialized table of contents for integrity checking. Every byte is
    // covered except the TocContentHash field itself, which would otherwise be
    // self-referential. Writer and reader must both use this so they cannot drift.
    uint64_t ComputePackageTocHash(const void* tocData, size_t tocSize);

    // Converts a path to the canonical archive form: relative, forward slashes,
    // no leading "./". Returns an empty string for paths that must not be
    // packaged or looked up (absolute, empty, or containing "..").
    std::string MakePackageEntryName(const std::filesystem::path& path);

    // True when the entry name is safe to resolve inside a package. Protects the
    // mount point from crafted packages that try to escape it.
    bool IsSafePackageEntryName(std::string_view entryName);

    // Lowercases ASCII so lookups match Windows path semantics. Archive entries
    // are compared case-insensitively by the virtual file system.
    std::string ToLowerAscii(std::string_view text);

    // Splits a forward-slash entry name into its parent directory, or an empty
    // string when the entry sits at the package root.
    std::string GetPackageEntryDirectory(std::string_view entryName);
}
