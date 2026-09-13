#pragma once

#include "Asset/AssetDatabase.h"
#include "Core/Base.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // File browser rooted at the project Assets directory, shown as a large-icon grid.
    //
    // Every file operation - create, rename, duplicate, delete, move - goes through the
    // AssetDatabase, so a sidecar record travels with its asset and existing references keep
    // working. The panel owns the UI state; the database owns the consequences, and the actual
    // operation runs after the grid so the file listing does not change under the loop that
    // produced it.
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
        void DrawToolbar(EditorContext& context, const std::filesystem::path& assetsDirectory);
        void DrawRenamePopup(EditorContext& context);
        void DrawDeleteConfirm(EditorContext& context);
        void DrawContextMenu(EditorContext& context, const std::filesystem::path& path);

        // Opens the double-clicked path: a folder navigates, a scene switches, an asset selects, and
        // anything else goes to the system.
        void ActivatePath(EditorContext& context, const std::filesystem::path& path);

        // Runs the operations the context menu and drag-drop requested.
        void ProcessPendingOperations(EditorContext& context);

        void BeginRename(const std::filesystem::path& path);
        void RequestDelete(const std::filesystem::path& path);
        void ReportResult(const std::string& operation, const std::filesystem::path& path, AssetWriteResult result);

    private:
        EditorLayer* m_Owner = nullptr;

        std::filesystem::path m_CurrentDirectory;
        std::vector<std::filesystem::path> m_BackHistory;
        std::vector<std::filesystem::path> m_ForwardHistory;
        std::filesystem::path m_SelectedPath;

        // Inline rename state: the path being renamed, its current text, and whether the field still
        // needs focus.
        std::filesystem::path m_RenamingPath;
        std::string m_RenameBuffer;
        bool m_RenameFocusPending = false;

        std::filesystem::path m_DeleteRequestPath;
        bool m_DeleteConfirmOpen = false;

        std::filesystem::path m_PendingMoveSource;
        std::filesystem::path m_PendingMoveTarget;
        std::filesystem::path m_PendingDuplicate;
        std::filesystem::path m_PendingFindReferences;
        std::filesystem::path m_PendingReveal;
        std::filesystem::path m_PendingOpen;
    };
}
