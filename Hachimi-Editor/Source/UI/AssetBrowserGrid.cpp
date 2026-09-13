#include "UI/AssetBrowserGrid.h"

#include "Asset/AssetDatabase.h"
#include "Asset/TextureCache.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Renderer/Texture.h"
#include "Utils/FileSystem.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>

namespace HachimiEngine
{
    namespace
    {
        constexpr ImVec2 ItemSize { 96.0f, 112.0f };
        constexpr float IconPadding = 8.0f;
        constexpr float IconSize = 64.0f;
        constexpr float LabelAreaHeight = 32.0f;

        constexpr const char* FolderGlyph = "\uE8B7";
        constexpr const char* SceneGlyph = "\uE8A5";
        constexpr const char* ScriptGlyph = "\uE943";
        constexpr const char* ImageGlyph = "\uEB9F";
        constexpr const char* MaterialGlyph = "\uE790";
        constexpr const char* FileGlyph = "\uE7C3";

        std::string GetLowerExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return extension;
        }

        const char* GetFileGlyph(const std::filesystem::path& path)
        {
            const std::string extension = GetLowerExtension(path);
            if (extension == ".hscene")
            {
                return SceneGlyph;
            }
            if (extension == ".hmaterial")
            {
                return MaterialGlyph;
            }
            if (extension == ".lua")
            {
                return ScriptGlyph;
            }
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
                || extension == ".tga" || extension == ".bmp")
            {
                return ImageGlyph;
            }
            return FileGlyph;
        }

        ImVec2 GetGlyphSize(const char* glyph, float fontSize)
        {
            return ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, glyph);
        }

        void DrawGlyph(ImDrawList* drawList, const char* glyph, const ImVec2& areaMin, const ImVec2& areaSize, ImU32 color)
        {
            const float fontSize = std::min(ImGui::GetFontSize() * 2.2f, areaSize.y * 0.62f);
            const ImVec2 glyphSize = GetGlyphSize(glyph, fontSize);
            const ImVec2 glyphPosition {
                areaMin.x + (areaSize.x - glyphSize.x) * 0.5f,
                areaMin.y + (areaSize.y - glyphSize.y) * 0.5f
            };

            drawList->AddText(ImGui::GetFont(), fontSize, glyphPosition, color, glyph);
        }

        // Draws an image thumbnail letterboxed inside the icon area. UVs are flipped for OpenGL textures.
        void DrawTextureThumbnail(ImDrawList* drawList, const Ref<Texture2D>& texture, const ImVec2& areaMin, const ImVec2& areaSize)
        {
            const float imageAspect = static_cast<float>(texture->GetWidth()) / static_cast<float>(std::max(texture->GetHeight(), 1u));
            const float areaAspect = areaSize.x / areaSize.y;

            ImVec2 imageSize = areaSize;
            if (imageAspect > areaAspect)
            {
                imageSize.y = areaSize.x / imageAspect;
            }
            else
            {
                imageSize.x = areaSize.y * imageAspect;
            }

            const ImVec2 imageMin {
                areaMin.x + (areaSize.x - imageSize.x) * 0.5f,
                areaMin.y + (areaSize.y - imageSize.y) * 0.5f
            };
            const ImVec2 imageMax { imageMin.x + imageSize.x, imageMin.y + imageSize.y };

            drawList->AddImage(
                static_cast<ImTextureID>(texture->GetRendererID()),
                imageMin,
                imageMax,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f),
                IM_COL32_WHITE);
        }

        // Tracks in-flight thumbnail decodes so a texture is requested once while
        // the grid keeps drawing a placeholder glyph for it.
        std::unordered_set<UUID>& GetPendingThumbnails()
        {
            static std::unordered_set<UUID> pending;
            return pending;
        }

        Ref<Texture2D> GetTextureThumbnail(const std::filesystem::path& path, const AssetBrowserGrid::Callbacks& callbacks)
        {
            if (callbacks.Database == nullptr || callbacks.Textures == nullptr)
            {
                return nullptr;
            }

            const std::optional<AssetHandle> handle = callbacks.Database->GetHandleForPath(path);
            if (!handle.has_value() || handle->Type != AssetType::Texture || !callbacks.Database->Contains(*handle))
            {
                return nullptr;
            }

            if (const Ref<Texture2D> cached = callbacks.Textures->GetCached(*handle))
            {
                return cached;
            }

            // Decoding runs on a worker thread; the texture appears through the cache once
            // TextureCache::PumpCompletedRequests() has uploaded it.
            std::unordered_set<UUID>& pending = GetPendingThumbnails();
            if (!pending.contains(handle->ID))
            {
                const AssetMeta* meta = callbacks.Database->GetMeta(*handle);
                const TextureImportSettings settings = meta != nullptr ? meta->Texture : TextureImportSettings();

                const uint64_t requestId = callbacks.Textures->Request(*handle, settings,
                    [id = handle->ID](AssetHandle, const Ref<Texture2D>& texture)
                    {
                        if (texture != nullptr)
                        {
                            GetPendingThumbnails().erase(id);
                        }
                    });

                if (requestId != 0)
                {
                    pending.insert(handle->ID);
                }
            }

            return nullptr;
        }

        void DrawItemIcon(ImDrawList* drawList, const std::filesystem::path& path, bool isDirectory,
                          const ImVec2& itemMin, const AssetBrowserGrid::Callbacks& callbacks)
        {
            const ImVec2 iconMin {
                itemMin.x + (ItemSize.x - IconSize) * 0.5f,
                itemMin.y + IconPadding
            };
            const ImVec2 iconSize { IconSize, IconSize };

            drawList->AddRectFilled(iconMin, { iconMin.x + iconSize.x, iconMin.y + iconSize.y },
                ImGui::GetColorU32(ImGuiCol_FrameBg), 0.0f);

            if (isDirectory)
            {
                DrawGlyph(drawList, FolderGlyph, iconMin, iconSize, ImGui::GetColorU32(ImGuiCol_Text));
                return;
            }

            if (AssetBrowserGrid::IsImagePath(path))
            {
                const Ref<Texture2D> thumbnail = GetTextureThumbnail(path, callbacks);
                if (thumbnail != nullptr)
                {
                    DrawTextureThumbnail(drawList, thumbnail, iconMin, iconSize);
                    return;
                }
            }

            DrawGlyph(drawList, GetFileGlyph(path), iconMin, iconSize, ImGui::GetColorU32(ImGuiCol_Text));
        }

        std::string TruncateLabel(const std::string& label, float availableWidth)
        {
            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            constexpr const char* Ellipsis = "...";

            const ImVec2 fullSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());
            if (fullSize.x <= availableWidth)
            {
                return label;
            }

            std::string truncated = label;
            while (truncated.size() > 1)
            {
                truncated.pop_back();
                const std::string candidate = truncated + Ellipsis;
                const ImVec2 candidateSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, candidate.c_str());
                if (candidateSize.x <= availableWidth)
                {
                    return candidate;
                }
            }

            return Ellipsis;
        }

        void DrawItemLabel(ImDrawList* drawList, const std::filesystem::path& path, const ImVec2& itemMin)
        {
            constexpr float HorizontalPadding = 6.0f;
            const float availableWidth = ItemSize.x - HorizontalPadding * 2.0f;
            std::string label = FileSystem::GetFileName(path);

            // A sidecar takes its asset's name without the extra extension: "Grid.png.meta" shows as
            // "Grid.png", which is the file it belongs to.
            if (path.extension() == ".meta")
            {
                label = path.stem().filename().string();
            }

            const std::string truncated = TruncateLabel(label, availableWidth);

            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            const ImVec2 labelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, truncated.c_str());

            const float labelTop = itemMin.y + IconPadding + IconSize + (LabelAreaHeight - labelSize.y) * 0.5f;
            const ImVec2 labelPosition {
                itemMin.x + (ItemSize.x - labelSize.x) * 0.5f,
                labelTop
            };

            drawList->AddText(font, fontSize, labelPosition, ImGui::GetColorU32(ImGuiCol_Text), truncated.c_str());
        }
    }

    bool AssetBrowserGrid::IsImagePath(const std::filesystem::path& path)
    {
        const std::string extension = GetLowerExtension(path);
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg"
            || extension == ".tga" || extension == ".bmp";
    }

    void AssetBrowserGrid::Draw(
        const std::vector<std::filesystem::path>& directories,
        const std::vector<std::filesystem::path>& files,
        std::filesystem::path& selectedPath,
        const Callbacks& callbacks)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columnCount = std::max(1, static_cast<int>((availableWidth + style.ItemSpacing.x) / (ItemSize.x + style.ItemSpacing.x)));

        if (!ImGui::BeginChild("##AssetBrowserGrid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int itemIndex = 0;

        const auto drawItem = [&](const std::filesystem::path& path, bool isDirectory)
        {
            if ((itemIndex % columnCount) != 0)
            {
                ImGui::SameLine();
            }

            ImGui::PushID(path.string().c_str());

            const bool wasSelected = selectedPath == path;
            const bool clicked = ImGui::Selectable(
                "##AssetBrowserItem",
                wasSelected,
                ImGuiSelectableFlags_AllowDoubleClick,
                ItemSize);
            const bool doubleClicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            if (clicked && !wasSelected)
            {
                selectedPath = path;
                if (callbacks.OnSelectionChanged)
                {
                    callbacks.OnSelectionChanged(path);
                }
            }

            // The context menu is filled by the caller, which owns what a file can do, and it is
            // drawn here so it opens next to the item that was right-clicked.
            if (callbacks.DrawContextMenu && ImGui::BeginPopupContextItem("##AssetItemMenu"))
            {
                callbacks.DrawContextMenu(path);
                ImGui::EndPopup();
            }

            const ImVec2 itemMin = ImGui::GetItemRectMin();
            DrawItemIcon(drawList, path, isDirectory, itemMin, callbacks);
            DrawItemLabel(drawList, path, itemMin);

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::string payloadPath = path.string();
                ImGui::SetDragDropPayload(FilePayload, payloadPath.c_str(), payloadPath.size() + 1);
                ImGui::TextUnformatted(FileSystem::GetFileName(path).c_str());
                ImGui::EndDragDropSource();
            }

            // A directory is a drop target, which is how an asset is moved into a folder.
            if (isDirectory && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FilePayload))
                {
                    const std::string droppedPath(static_cast<const char*>(payload->Data));
                    if (callbacks.OnDrop && std::filesystem::path(droppedPath) != path)
                    {
                        callbacks.OnDrop(droppedPath, path);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginItemTooltip())
            {
                ImGui::TextUnformatted(path.string().c_str());
                ImGui::EndTooltip();
            }

            if (doubleClicked && callbacks.OnActivate)
            {
                callbacks.OnActivate(path);
            }

            ImGui::PopID();
            ++itemIndex;
        };

        for (const auto& directory : directories)
        {
            drawItem(directory, true);
        }
        for (const auto& file : files)
        {
            drawItem(file, false);
        }

        // Empty space in the grid is a drop target too, meaning "move it into the folder shown".
        // An invisible button covering the remaining area is used rather than a custom rect, because
        // the custom-rect helper lives in ImGui's internal header.
        const ImVec2 remaining = ImGui::GetContentRegionAvail();
        if (remaining.x > 1.0f && remaining.y > 1.0f)
        {
            ImGui::InvisibleButton("##AssetBrowserGridDropZone", remaining);
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FilePayload))
                {
                    const std::string droppedPath(static_cast<const char*>(payload->Data));
                    if (callbacks.OnDrop)
                    {
                        callbacks.OnDrop(droppedPath, std::filesystem::path(droppedPath).parent_path());
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::EndChild();
    }
}
