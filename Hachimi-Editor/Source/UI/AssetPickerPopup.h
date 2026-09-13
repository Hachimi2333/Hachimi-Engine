#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    // Built-in modal asset picker used by inspector asset fields. It browses a root directory with
    // the same large-icon grid as the Content Browser and returns the selected absolute path.
    class AssetPickerPopup
    {
    public:
        void Open(
            std::string title,
            const std::filesystem::path& rootDirectory,
            const std::vector<std::string>& allowedExtensions);

        bool IsOpen() const { return m_Open; }
        void Close() { m_Open = false; }

        // True once the picker was dismissed without a selection, e.g. through Cancel or the close
        // button. Reading it clears the flag, so a panel can drop the field slot it was holding for
        // the picker instead of leaving it armed for the next one.
        bool ConsumeCancelled()
        {
            const bool cancelled = m_Cancelled;
            m_Cancelled = false;
            return cancelled;
        }

        // The file the user activated, valid after Draw() returned true. Drawers that resolve the
        // selection themselves read it here instead of keeping it in the panel.
        const std::filesystem::path& GetSelectedPath() const { return m_SelectedPath; }

        // Returns true when a file was selected. selectedPath is only written in that case.
        bool Draw(std::filesystem::path& selectedPath);

    private:
        void NavigateTo(const std::filesystem::path& directory);
        void NavigateBack();
        void NavigateUp();
        void DrawBreadcrumb();

    private:
        bool m_Open = false;
        bool m_Cancelled = false;
        // Tracks the popup's own visibility, so closing it from outside Draw() is recognised as a
        // cancellation on the next frame.
        bool m_WasOpen = false;
        std::string m_Title;
        std::filesystem::path m_RootDirectory;
        std::vector<std::string> m_AllowedExtensions;
        std::filesystem::path m_CurrentDirectory;
        std::vector<std::filesystem::path> m_BackHistory;
        std::filesystem::path m_GridSelectedPath;
        std::filesystem::path m_SelectedPath;
    };
}
