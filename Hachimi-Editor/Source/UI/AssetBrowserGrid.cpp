#include "UI/AssetBrowserGrid.h"

#include "Asset/AssetManager.h"
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
        constexpr const char* FileGlyph = "\uE7C3";

        std::string GetLowerExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return extension;
        }

        bool IsImageExtension(const std::string& extension)
        {
            return extension == ".png"
                || extension == ".jpg"
                || extension == ".jpeg"
                || extension == ".tga"
                || extension == ".bmp";
        }

        const char* GetFileGlyph(const std::filesystem::path& path)
        {
            const std::string extension = GetLowerExtension(path);
            if (extension == ".hscene")
            {
                return SceneGlyph;
            }
            if (extension == ".lua")
            {
                return ScriptGlyph;
            }
            if (IsImageExtension(extension))
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
        std::unordered_set<std::string>& GetPendingThumbnails()
        {
            static std::unordered_set<std::string> pending;
            return pending;
        }

        Ref<Texture2D> GetTextureThumbnail(const std::filesystem::path& path)
        {
            if (!IsImageExtension(GetLowerExtension(path)))
            {
                return nullptr;
            }

            const std::filesystem::path assetsDirectory = AssetManager::GetAssetsDirectory();
            std::error_code errorCode;
            const std::filesystem::path relativePath = std::filesystem::relative(path, assetsDirectory, errorCode);
            if (errorCode)
            {
                return nullptr;
            }

            if (Ref<Texture2D> cached = AssetManager::GetCachedTexture(relativePath))
            {
                return cached;
            }

            // Decoding runs on a worker thread; the texture appears through the
            // cache once AssetManager::PumpCompletedRequests() has uploaded it.
            const std::string key = relativePath.generic_string();
            std::unordered_set<std::string>& pending = GetPendingThumbnails();
            if (!pending.contains(key))
            {
                const uint64_t requestId = AssetManager::RequestTexture(relativePath,
                    [key](const std::filesystem::path&, const Ref<Texture2D>& texture)
                    {
                        if (texture != nullptr)
                        {
                            GetPendingThumbnails().erase(key);
                        }
                    });

                if (requestId != 0)
                {
                    pending.insert(key);
                }
            }

            return nullptr;
        }

        void DrawItemIcon(ImDrawList* drawList, const std::filesystem::path& path, bool isDirectory, const ImVec2& itemMin)
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

            const Ref<Texture2D> thumbnail = GetTextureThumbnail(path);
            if (thumbnail != nullptr)
            {
                DrawTextureThumbnail(drawList, thumbnail, iconMin, iconSize);
                return;
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
            const std::string label = TruncateLabel(FileSystem::GetFileName(path), availableWidth);

            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            const ImVec2 labelSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());

            const float labelTop = itemMin.y + IconPadding + IconSize + (LabelAreaHeight - labelSize.y) * 0.5f;
            const ImVec2 labelPosition {
                itemMin.x + (ItemSize.x - labelSize.x) * 0.5f,
                labelTop
            };

            drawList->AddText(font, fontSize, labelPosition, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
        }
    }

    void AssetBrowserGrid::Draw(
        const std::vector<std::filesystem::path>& directories,
        const std::vector<std::filesystem::path>& files,
        std::filesystem::path& selectedPath,
        std::filesystem::path& activatedPath)
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

            if (clicked)
            {
                selectedPath = path;
            }

            const ImVec2 itemMin = ImGui::GetItemRectMin();
            DrawItemIcon(drawList, path, isDirectory, itemMin);
            DrawItemLabel(drawList, path, itemMin);

            if (!isDirectory && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::string payloadPath = path.string();
                ImGui::SetDragDropPayload(FilePayload, payloadPath.c_str(), payloadPath.size() + 1);
                ImGui::TextUnformatted(FileSystem::GetFileName(path).c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginItemTooltip())
            {
                ImGui::TextUnformatted(path.string().c_str());
                ImGui::EndTooltip();
            }

            if (doubleClicked)
            {
                activatedPath = path;
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

        ImGui::EndChild();
    }
}
