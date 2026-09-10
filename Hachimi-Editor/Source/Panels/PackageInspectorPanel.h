#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Packaging/PackageReader.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Dockable panel that opens a .hpak and inspects it without extracting it:
    // lists the table of contents, shows header and compression statistics, can
    // verify every entry against its content hash and export single entries.
    class PackageInspectorPanel
    {
    public:
        void Draw(EditorLayer* owner, EditorContext& context);

        // Opens a package and refreshes the listing. Logs and keeps the previous
        // state on failure.
        bool OpenPackage(const std::filesystem::path& packagePath);

    private:
        struct EntryRow
        {
            std::string Path;
            uint64_t UncompressedSize = 0;
            uint64_t CompressedSize = 0;
            uint64_t ContentHash = 0;
            uint32_t BlockCount = 0;
            bool Zstd = false;
        };

        bool ExportEntry(const EntryRow& row, const std::filesystem::path& destinationPath) const;

        Scope<PackageReader> m_Reader;
        std::filesystem::path m_PackagePath;
        std::vector<EntryRow> m_Entries;
        std::string m_StatusMessage;
        bool m_StatusIsError = false;
        char m_Filter[128] = {};
        int m_SelectedEntry = -1;
    };
}
