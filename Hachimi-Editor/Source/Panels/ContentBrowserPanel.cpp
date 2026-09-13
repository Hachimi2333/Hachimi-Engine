#include "Panels/ContentBrowserPanel.h"

#include "Asset/AssetDatabase.h"
#include "Asset/MaterialAsset.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"
#include "Project/ProjectManager.h"
#include "UI/AssetBrowserGrid.h"
#include "Utils/FileDialogs.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* BackGlyph = "\uE72B";
        constexpr const char* ForwardGlyph = "\uE72A";
        constexpr const char* UpGlyph = "\uE74A";

        bool DrawToolbarButton(const char* glyph, const char* tooltip, bool enabled)
        {
            ImGui::BeginDisabled(!enabled);
            const bool clicked = ImGui::Button(glyph, ImVec2(ImGui::GetFrameHeight() * 1.6f, 0.0f));
            ImGui::EndDisabled();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", tooltip);
            }

            return clicked;
        }

        std::filesystem::path FindUniquePath(const std::filesystem::path& directory, const std::string& stem,
                                             const std::string& extension)
        {
            std::filesystem::path candidate = directory / (stem + extension);
            for (int suffix = 1; FileSystem::Exists(candidate); ++suffix)
            {
                candidate = directory / (stem + "_" + std::to_string(suffix) + extension);
            }
            return candidate;
        }
    }

    void ContentBrowserPanel::NavigateTo(const std::filesystem::path& directory)
    {
        if (m_CurrentDirectory == directory)
        {
            return;
        }

        if (!m_CurrentDirectory.empty())
        {
            m_BackHistory.push_back(m_CurrentDirectory);
        }
        m_ForwardHistory.clear();
        m_CurrentDirectory = directory;
        m_SelectedPath.clear();
        m_RenamingPath.clear();
    }

    void ContentBrowserPanel::NavigateBack()
    {
        if (m_BackHistory.empty())
        {
            return;
        }

        m_ForwardHistory.push_back(m_CurrentDirectory);
        m_CurrentDirectory = m_BackHistory.back();
        m_BackHistory.pop_back();
        m_SelectedPath.clear();
    }

    void ContentBrowserPanel::NavigateForward()
    {
        if (m_ForwardHistory.empty())
        {
            return;
        }

        m_BackHistory.push_back(m_CurrentDirectory);
        m_CurrentDirectory = m_ForwardHistory.back();
        m_ForwardHistory.pop_back();
        m_SelectedPath.clear();
    }

    void ContentBrowserPanel::NavigateUp(const std::filesystem::path& assetsDirectory)
    {
        if (m_CurrentDirectory == assetsDirectory)
        {
            return;
        }

        NavigateTo(m_CurrentDirectory.parent_path());
    }

    void ContentBrowserPanel::DrawBreadcrumb(const std::filesystem::path& assetsDirectory)
    {
        if (m_CurrentDirectory == assetsDirectory)
        {
            ImGui::TextUnformatted("Assets");
            return;
        }

        ImGui::TextUnformatted("Assets");

        const std::filesystem::path relativePath = m_CurrentDirectory.lexically_relative(assetsDirectory);
        std::filesystem::path accumulatedPath = assetsDirectory;
        const size_t componentCount = std::distance(relativePath.begin(), relativePath.end());

        size_t componentIndex = 0;
        for (const auto& component : relativePath)
        {
            accumulatedPath /= component;

            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();

            if (componentIndex + 1 < componentCount)
            {
                if (ImGui::Button(component.string().c_str()))
                {
                    NavigateTo(accumulatedPath);
                }
            }
            else
            {
                ImGui::TextUnformatted(component.string().c_str());
            }

            ++componentIndex;
        }
    }

    void ContentBrowserPanel::ReportResult(const std::string& operation, const std::filesystem::path& path,
                                           AssetWriteResult result)
    {
        if (result == AssetWriteResult::Success)
        {
            HE_CLIENT_INFO("{}: {}", operation, path.filename().string());
        }
        else
        {
            HE_CLIENT_ERROR("{} failed for '{}': {}", operation, path.filename().string(), ToString(result));
        }
    }

    void ContentBrowserPanel::BeginRename(const std::filesystem::path& path)
    {
        m_RenamingPath = path;
        m_RenameBuffer = path.filename().string();
        m_RenameFocusPending = true;
    }

    void ContentBrowserPanel::RequestDelete(const std::filesystem::path& path)
    {
        m_DeleteRequestPath = path;
        m_DeleteConfirmOpen = true;
    }

    void ContentBrowserPanel::DrawToolbar(EditorContext& context, const std::filesystem::path& assetsDirectory)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float importButtonWidth = ImGui::CalcTextSize("Import Asset").x + style.FramePadding.x * 2.0f;

        if (DrawToolbarButton(BackGlyph, "Back", !m_BackHistory.empty()))
        {
            NavigateBack();
        }
        ImGui::SameLine();
        if (DrawToolbarButton(ForwardGlyph, "Forward", !m_ForwardHistory.empty()))
        {
            NavigateForward();
        }
        ImGui::SameLine();
        if (DrawToolbarButton(UpGlyph, "Up", m_CurrentDirectory != assetsDirectory))
        {
            NavigateUp(assetsDirectory);
        }

        ImGui::SameLine();
        if (ImGui::Button("New Folder"))
        {
            if (context.Assets != nullptr)
            {
                std::filesystem::path created;
                const AssetWriteResult result =
                    context.Assets->CreateDirectory(m_CurrentDirectory, "New Folder", created);
                if (result == AssetWriteResult::Success)
                {
                    BeginRename(created);
                }
                else
                {
                    ReportResult("Create folder", m_CurrentDirectory, result);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("New Material"))
        {
            if (context.Assets != nullptr)
            {
                AssetHandle created;
                const AssetWriteResult result = context.Assets->CreateAsset(
                    m_CurrentDirectory,
                    "NewMaterial",
                    AssetType::Material,
                    MaterialAsset::Serialize(MaterialAsset::Default()),
                    created);

                if (result == AssetWriteResult::Success)
                {
                    m_SelectedPath = context.Assets->GetAssetPath(created);
                }
                else
                {
                    ReportResult("Create material", m_CurrentDirectory, result);
                }
            }
        }

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - importButtonWidth - style.WindowPadding.x);
        if (ImGui::Button("Import Asset", ImVec2(importButtonWidth, 0.0f)))
        {
            const std::filesystem::path selectedPath = FileDialogs::OpenAssetImportDialog(assetsDirectory);
            if (!selectedPath.empty() && context.Assets != nullptr)
            {
                AssetHandle imported;
                const AssetWriteResult result = context.Assets->ImportAsset(selectedPath, m_CurrentDirectory, imported);
                if (result == AssetWriteResult::Success)
                {
                    m_SelectedPath = context.Assets->GetAssetPath(imported);
                }
                else
                {
                    ReportResult("Import", selectedPath, result);
                }
            }
        }
    }

    void ContentBrowserPanel::DrawContextMenu(EditorContext& context, const std::filesystem::path& path)
    {
        (void)context;

        const bool isDirectory = FileSystem::IsDirectory(path);

        ImGui::TextDisabled("%s", path.filename().string().c_str());
        ImGui::Separator();

        if (!isDirectory && ImGui::MenuItem("Open"))
        {
            m_PendingOpen = path;
        }

        if (ImGui::MenuItem("Rename"))
        {
            BeginRename(path);
        }

        ImGui::BeginDisabled(isDirectory);
        if (ImGui::MenuItem("Duplicate"))
        {
            m_PendingDuplicate = path;
        }

        if (ImGui::MenuItem("Find References"))
        {
            m_PendingFindReferences = path;
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        if (ImGui::MenuItem("Delete"))
        {
            RequestDelete(path);
        }

        if (ImGui::MenuItem("Show in Explorer"))
        {
            m_PendingReveal = path;
        }
    }

    void ContentBrowserPanel::DrawRenamePopup(EditorContext& context)
    {
        if (m_RenamingPath.empty())
        {
            return;
        }

        ImGui::OpenPopup("Rename###RenameAsset");

        if (!ImGui::BeginPopupModal("Rename###RenameAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        ImGui::TextUnformatted("New name");
        ImGui::SetNextItemWidth(320.0f);
        if (m_RenameFocusPending)
        {
            ImGui::SetKeyboardFocusHere();
            m_RenameFocusPending = false;
        }

        const bool submitted = ImGui::InputText("##RenameBuffer", m_RenameBuffer.data(), m_RenameBuffer.capacity() + 1,
            ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("Rename") || submitted)
        {
            if (context.Assets != nullptr && !m_RenameBuffer.empty())
            {
                const AssetWriteResult result = FileSystem::IsDirectory(m_RenamingPath)
                    ? [&]
                    {
                        std::filesystem::path renamed;
                        return context.Assets->RenameDirectory(m_RenamingPath, m_RenameBuffer, renamed);
                    }()
                    : [&]
                    {
                        const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_RenamingPath);
                        return handle.has_value()
                            ? context.Assets->Rename(*handle, m_RenameBuffer)
                            : AssetWriteResult::NotFound;
                    }();

                ReportResult("Rename", m_RenamingPath, result);
                if (result == AssetWriteResult::Success)
                {
                    m_SelectedPath.clear();
                }
            }

            m_RenamingPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_RenamingPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ContentBrowserPanel::DrawDeleteConfirm(EditorContext& context)
    {
        if (!m_DeleteConfirmOpen)
        {
            return;
        }

        ImGui::OpenPopup("Delete###DeleteAsset");

        if (!ImGui::BeginPopupModal("Delete###DeleteAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        ImGui::TextUnformatted("Delete this asset?");
        ImGui::TextDisabled("%s", m_DeleteRequestPath.filename().string().c_str());

        // Whoever still points at the asset is named before it goes: a delete that silently breaks a
        // scene is the failure this whole reference index exists to prevent.
        if (context.Assets != nullptr && !FileSystem::IsDirectory(m_DeleteRequestPath))
        {
            const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_DeleteRequestPath);
            if (handle.has_value())
            {
                const std::vector<std::filesystem::path> references = context.Assets->FindReferences(*handle);
                if (!references.empty())
                {
                    ImGui::Spacing();
                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Referenced by:");
                    for (const std::filesystem::path& reference : references)
                    {
                        ImGui::BulletText("%s", reference.generic_string().c_str());
                    }
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
        {
            if (context.Assets != nullptr)
            {
                const AssetWriteResult result = FileSystem::IsDirectory(m_DeleteRequestPath)
                    ? context.Assets->DeleteDirectory(m_DeleteRequestPath)
                    : [&]
                    {
                        const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_DeleteRequestPath);
                        return handle.has_value()
                            ? context.Assets->Delete(*handle)
                            : AssetWriteResult::NotFound;
                    }();

                ReportResult("Delete", m_DeleteRequestPath, result);
                m_SelectedPath.clear();
            }

            m_DeleteRequestPath.clear();
            m_DeleteConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            m_DeleteRequestPath.clear();
            m_DeleteConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ContentBrowserPanel::ProcessPendingOperations(EditorContext& context)
    {
        if (context.Assets == nullptr)
        {
            return;
        }

        if (!m_PendingMoveSource.empty())
        {
            const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_PendingMoveSource);
            if (handle.has_value())
            {
                ReportResult("Move", m_PendingMoveSource,
                    context.Assets->Move(*handle, m_PendingMoveTarget));
            }
            m_PendingMoveSource.clear();
        }

        if (!m_PendingDuplicate.empty())
        {
            const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_PendingDuplicate);
            if (handle.has_value())
            {
                AssetHandle copy;
                const AssetWriteResult result = context.Assets->Duplicate(*handle, copy);
                ReportResult("Duplicate", m_PendingDuplicate, result);
                if (result == AssetWriteResult::Success)
                {
                    m_SelectedPath = context.Assets->GetAssetPath(copy);
                }
            }
            m_PendingDuplicate.clear();
        }

        if (!m_PendingFindReferences.empty())
        {
            const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(m_PendingFindReferences);
            if (handle.has_value())
            {
                const std::vector<std::filesystem::path> references = context.Assets->FindReferences(*handle);
                if (references.empty())
                {
                    HE_CLIENT_INFO("Nothing references {}", m_PendingFindReferences.filename().string());
                }
                for (const std::filesystem::path& reference : references)
                {
                    HE_CLIENT_INFO("{} is referenced by {}", m_PendingFindReferences.filename().string(),
                        reference.generic_string());
                }
            }
            m_PendingFindReferences.clear();
        }

        if (!m_PendingReveal.empty())
        {
            PlatformUtils::OpenPathInExplorer(m_PendingReveal);
            m_PendingReveal.clear();
        }

        if (!m_PendingOpen.empty())
        {
            const std::filesystem::path path = m_PendingOpen;
            m_PendingOpen.clear();
            ActivatePath(context, path);
        }
    }

    void ContentBrowserPanel::ActivatePath(EditorContext& context, const std::filesystem::path& path)
    {
        if (FileSystem::IsDirectory(path))
        {
            NavigateTo(path);
            return;
        }

        if (FileSystem::GetExtension(path) == ".hscene")
        {
            // Opening a scene goes through the editor layer, because it has to respect unsaved
            // changes and stop the runtime first.
            if (context.Assets != nullptr && context.Assets->Contains(context.SelectedAsset))
            {
                m_Owner->SwitchToScene(path);
                return;
            }
        }

        if (context.Assets == nullptr)
        {
            return;
        }

        const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(path);
        if (handle.has_value())
        {
            // Selecting an asset is what puts the Inspector into asset mode; the panel does not
            // need to know which asset kinds have an editor.
            context.SelectAsset(*handle, path);
            return;
        }

        // Not a project asset: hand it to the system.
        PlatformUtils::OpenPathInExplorer(path);
    }

    void ContentBrowserPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        if (!ImGui::Begin("Content Browser"))
        {
            ImGui::End();
            return;
        }

        m_Owner = owner;

        AssetDatabase* assets = context.Assets;
        if (assets == nullptr)
        {
            ImGui::TextDisabled("No project is open");
            ImGui::End();
            return;
        }

        const std::filesystem::path assetsDirectory = assets->GetAssetsDirectory();
        if (m_CurrentDirectory.empty())
        {
            m_CurrentDirectory = assetsDirectory;
        }

        if (!FileSystem::IsDirectory(m_CurrentDirectory))
        {
            m_CurrentDirectory = assetsDirectory;
            m_BackHistory.clear();
            m_ForwardHistory.clear();
            m_SelectedPath.clear();
        }

        DrawToolbar(context, assetsDirectory);
        DrawBreadcrumb(assetsDirectory);
        ImGui::Separator();

        // Inline rename replaces the item's label for as long as it is active, so the user types
        // where the name already is.
        if (!m_RenamingPath.empty() && m_RenamingPath.parent_path() == m_CurrentDirectory)
        {
            ImGui::TextUnformatted("Renaming:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(320.0f);
            if (m_RenameFocusPending)
            {
                ImGui::SetKeyboardFocusHere();
                m_RenameFocusPending = false;
            }

            const bool submitted = ImGui::InputText("##InlineRename", m_RenameBuffer.data(),
                m_RenameBuffer.capacity() + 1, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            if (ImGui::Button("OK") || submitted)
            {
                if (!m_RenameBuffer.empty())
                {
                    AssetWriteResult result = AssetWriteResult::NotFound;
                    if (FileSystem::IsDirectory(m_RenamingPath))
                    {
                        std::filesystem::path renamed;
                        result = assets->RenameDirectory(m_RenamingPath, m_RenameBuffer, renamed);
                    }
                    else if (const std::optional<AssetHandle> handle = assets->GetHandleForPath(m_RenamingPath);
                             handle.has_value())
                    {
                        result = assets->Rename(*handle, m_RenameBuffer);
                    }

                    ReportResult("Rename", m_RenamingPath, result);
                    if (result == AssetWriteResult::Success)
                    {
                        m_SelectedPath.clear();
                    }
                }

                m_RenamingPath.clear();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_RenamingPath.clear();
            }

            ImGui::Separator();
        }

        std::vector<std::filesystem::path> directories = FileSystem::GetDirectories(m_CurrentDirectory);
        std::vector<std::filesystem::path> files;
        for (const std::filesystem::path& file : FileSystem::GetFiles(m_CurrentDirectory))
        {
            // A sidecar record is not content: it belongs to its asset, not to the browser.
            if (file.extension() != ".meta")
            {
                files.push_back(file);
            }
        }

        // The grid has to know which item was activated before it is drawn again next frame; the
        // action itself runs after the listing.
        std::filesystem::path activatedPath;

        AssetBrowserGrid::Callbacks callbacks;
        callbacks.Database = assets;
        callbacks.Textures = context.Textures;
        callbacks.OnActivate = [&activatedPath](const std::filesystem::path& path) { activatedPath = path; };
        callbacks.OnSelectionChanged = [this, &context](const std::filesystem::path& path)
        {
            m_SelectedPath = path;
            if (FileSystem::IsDirectory(path) || context.Assets == nullptr)
            {
                return;
            }

            const std::optional<AssetHandle> handle = context.Assets->GetHandleForPath(path);
            if (handle.has_value())
            {
                context.SelectAsset(*handle, path);
            }
        };
        callbacks.OnDrop = [this](const std::filesystem::path& droppedPath, const std::filesystem::path& targetDirectory)
        {
            m_PendingMoveSource = droppedPath;
            m_PendingMoveTarget = targetDirectory;
        };
        callbacks.DrawContextMenu = [this, &context](const std::filesystem::path& path)
        {
            m_SelectedPath = path;
            DrawContextMenu(context, path);
        };

        AssetBrowserGrid::Draw(directories, files, m_SelectedPath, callbacks);

        // A rename or delete started from the context menu opens its own modal.
        DrawRenamePopup(context);
        DrawDeleteConfirm(context);

        if (!activatedPath.empty())
        {
            ActivatePath(context, activatedPath);
        }

        ProcessPendingOperations(context);

        ImGui::End();
    }
}
