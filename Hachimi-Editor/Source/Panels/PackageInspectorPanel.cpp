#include "Panels/PackageInspectorPanel.h"

#include "Core/Log.h"
#include "Utils/FileDialogs.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        std::string FormatBytes(uint64_t bytes)
        {
            char buffer[64] = {};
            if (bytes >= 1024ull * 1024ull)
            {
                std::snprintf(buffer, sizeof(buffer), "%.2f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
            }
            else if (bytes >= 1024ull)
            {
                std::snprintf(buffer, sizeof(buffer), "%.2f KiB", static_cast<double>(bytes) / 1024.0);
            }
            else
            {
                std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
            }
            return buffer;
        }

        bool MatchesFilter(const std::string& text, const char* filter)
        {
            if (filter == nullptr || filter[0] == '\0')
            {
                return true;
            }

            std::string loweredText = ToLowerAscii(text);
            std::string loweredFilter = ToLowerAscii(filter);
            return loweredText.find(loweredFilter) != std::string::npos;
        }
    }

    bool PackageInspectorPanel::OpenPackage(const std::filesystem::path& packagePath)
    {
        Scope<PackageReader> reader = CreateScope<PackageReader>();
        if (!reader->Open(packagePath))
        {
            m_StatusMessage = "Failed to open package: " + packagePath.string();
            m_StatusIsError = true;
            HE_CLIENT_ERROR("{}", m_StatusMessage);
            return false;
        }

        m_Reader = std::move(reader);
        m_PackagePath = packagePath;
        m_SelectedEntry = -1;
        m_Entries.clear();
        m_Entries.reserve(m_Reader->GetEntryCount());

        uint64_t totalCompressed = 0;
        for (size_t index = 0; index < m_Reader->GetEntryCount(); ++index)
        {
            const PackageEntryInfo& entry = m_Reader->GetEntry(index);
            EntryRow row;
            row.Path = entry.VirtualPath.generic_string();
            row.UncompressedSize = entry.UncompressedSize;
            row.CompressedSize = entry.CompressedSize;
            row.ContentHash = entry.ContentHash;
            row.BlockCount = entry.BlockCount;
            row.Zstd = entry.Method == PackageCompressionMethod::Zstd;
            m_Entries.push_back(std::move(row));
            totalCompressed += entry.CompressedSize;
        }

        m_StatusMessage = "Opened " + packagePath.filename().string() + ": "
            + std::to_string(m_Entries.size()) + " entries, " + FormatBytes(totalCompressed) + " packed";
        m_StatusIsError = false;
        return true;
    }

    bool PackageInspectorPanel::ExportEntry(const EntryRow& row, const std::filesystem::path& destinationPath) const
    {
        if (m_Reader == nullptr)
        {
            return false;
        }

        size_t entryIndex = 0;
        if (!m_Reader->FindEntry(row.Path, entryIndex))
        {
            return false;
        }

        std::vector<uint8_t> data;
        if (!m_Reader->ReadEntry(entryIndex, data))
        {
            return false;
        }

        return FileSystem::WriteBinaryFile(destinationPath, data.data(), data.size());
    }

    void PackageInspectorPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        (void)owner;
        (void)context;

        if (!ImGui::Begin("Package Inspector"))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Open Package..."))
        {
            const std::filesystem::path startPath = m_PackagePath.empty()
                ? PlatformUtils::GetExecutableDirectory()
                : m_PackagePath.parent_path();
            const std::filesystem::path selected = FileDialogs::OpenPackageFileDialog(startPath);
            if (!selected.empty())
            {
                OpenPackage(selected);
            }
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Reader == nullptr);
        if (ImGui::Button("Verify All"))
        {
            PackageVerificationReport report;
            const bool ok = m_Reader->VerifyAll(report);
            m_StatusIsError = !ok;
            m_StatusMessage = "Verified " + std::to_string(report.VerifiedCount) + "/"
                + std::to_string(report.EntryCount) + " entries, "
                + std::to_string(report.FailedCount) + " failed";
            for (const std::string& failure : report.Failures)
            {
                HE_CLIENT_ERROR("Package entry failed verification: {}", failure);
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##filter", "Filter path", m_Filter, sizeof(m_Filter));

        if (!m_StatusMessage.empty())
        {
            if (m_StatusIsError)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", m_StatusMessage.c_str());
            }
            else
            {
                ImGui::TextDisabled("%s", m_StatusMessage.c_str());
            }
        }

        if (m_Reader != nullptr)
        {
            const PackageFileHeader& header = m_Reader->GetHeader();
            ImGui::SeparatorText("Package");
            ImGui::Text("Path: %s", m_PackagePath.string().c_str());
            ImGui::Text("Format version: %u   Block size: %u   Dictionary: %u bytes",
                header.FormatVersion, header.BlockSize, m_Reader->GetDictionarySize());
            ImGui::Text("Memory mapped: %s   Package id: 0x%016llX",
                m_Reader->IsMemoryMapped() ? "yes" : "no",
                static_cast<unsigned long long>(m_Reader->GetPackageId()));
        }

        ImGui::SeparatorText("Contents");

        if (m_Reader == nullptr)
        {
            ImGui::TextDisabled("No package open. Use Open Package... or export one from Build Settings.");
            ImGui::End();
            return;
        }

        uint64_t visibleUncompressed = 0;
        uint64_t visibleCompressed = 0;

        if (ImGui::BeginTable("##entries", 5,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
                | ImGuiTableFlags_Resizable, ImVec2(0.0f, -180.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Packed", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Method", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Blocks", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (size_t index = 0; index < m_Entries.size(); ++index)
            {
                const EntryRow& row = m_Entries[index];
                if (!MatchesFilter(row.Path, m_Filter))
                {
                    continue;
                }

                visibleUncompressed += row.UncompressedSize;
                visibleCompressed += row.CompressedSize;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(index));
                if (ImGui::Selectable(row.Path.c_str(), m_SelectedEntry == static_cast<int>(index),
                        ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_SelectedEntry = static_cast<int>(index);
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(FormatBytes(row.UncompressedSize).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(FormatBytes(row.CompressedSize).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.Zstd ? "zstd" : "store");
                ImGui::TableNextColumn();
                ImGui::Text("%u", row.BlockCount);
            }

            ImGui::EndTable();
        }

        ImGui::Text("Visible: %s raw -> %s packed", FormatBytes(visibleUncompressed).c_str(),
            FormatBytes(visibleCompressed).c_str());
        if (visibleUncompressed > 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(%.1f%%)",
                100.0 * static_cast<double>(visibleCompressed) / static_cast<double>(visibleUncompressed));
        }

        ImGui::BeginDisabled(m_SelectedEntry < 0 || m_SelectedEntry >= static_cast<int>(m_Entries.size()));
        if (ImGui::Button("Extract Entry..."))
        {
            const EntryRow& row = m_Entries[static_cast<size_t>(m_SelectedEntry)];
            const std::filesystem::path destination = FileDialogs::SaveFileDialog(
                PlatformUtils::GetExecutableDirectory(), std::filesystem::path(row.Path).filename().string());
            if (!destination.empty())
            {
                const bool ok = ExportEntry(row, destination);
                m_StatusIsError = !ok;
                m_StatusMessage = ok
                    ? "Extracted " + row.Path + " to " + destination.string()
                    : "Failed to extract " + row.Path;
            }
        }
        ImGui::EndDisabled();

        if (m_SelectedEntry >= 0 && m_SelectedEntry < static_cast<int>(m_Entries.size()))
        {
            const EntryRow& row = m_Entries[static_cast<size_t>(m_SelectedEntry)];
            ImGui::Spacing();
            ImGui::SeparatorText("Selected entry");
            ImGui::Text("Path: %s", row.Path.c_str());
            ImGui::Text("Raw: %s   Packed: %s   Blocks: %u",
                FormatBytes(row.UncompressedSize).c_str(), FormatBytes(row.CompressedSize).c_str(), row.BlockCount);
            ImGui::Text("Content hash: 0x%016llX", static_cast<unsigned long long>(row.ContentHash));
        }

        ImGui::End();
    }
}
