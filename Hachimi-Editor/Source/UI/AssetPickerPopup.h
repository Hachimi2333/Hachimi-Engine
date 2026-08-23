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

        // Returns true when a file was selected. selectedPath is only written in that case.
        bool Draw(std::filesystem::path& selectedPath);

    private:
        void NavigateTo(const std::filesystem::path& directory);
        void NavigateBack();
        void NavigateUp();
        void DrawBreadcrumb();

    private:
        bool m_Open = false;
        std::string m_Title;
        std::filesystem::path m_RootDirectory;
        std::vector<std::string> m_AllowedExtensions;
        std::filesystem::path m_CurrentDirectory;
        std::vector<std::filesystem::path> m_BackHistory;
        std::filesystem::path m_GridSelectedPath;
    };
}
