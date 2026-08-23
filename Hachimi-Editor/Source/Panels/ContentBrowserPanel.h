#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <vector>

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // File browser rooted at the project Assets directory, shown as a large-icon grid.
    class ContentBrowserPanel
    {
    public:
        void Draw(EditorLayer* owner, EditorContext& context);

    private:
        void NavigateTo(const std::filesystem::path& directory);
        void NavigateBack();
        void NavigateForward();
        void NavigateUp(const std::filesystem::path& assetsDirectory);
        void DrawBreadcrumb(const std::filesystem::path& assetsDirectory);

    private:
        std::filesystem::path m_CurrentDirectory;
        std::vector<std::filesystem::path> m_BackHistory;
        std::vector<std::filesystem::path> m_ForwardHistory;
        std::filesystem::path m_SelectedPath;
    };
}
