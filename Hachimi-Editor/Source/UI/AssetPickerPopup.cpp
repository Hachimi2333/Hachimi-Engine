#include "UI/AssetPickerPopup.h"

#include "UI/AssetBrowserGrid.h"
#include "Utils/FileSystem.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* BackGlyph = "\uE72B";
        constexpr const char* UpGlyph = "\uE74A";

        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        bool HasAllowedExtension(const std::filesystem::path& path, const std::vector<std::string>& allowedExtensions)
        {
            if (allowedExtensions.empty())
            {
                return true;
            }

            const std::string extension = ToLower(path.extension().string());
            return std::find(allowedExtensions.begin(), allowedExtensions.end(), extension) != allowedExtensions.end();
        }

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
    }

    void AssetPickerPopup::Open(
        std::string title,
        const std::filesystem::path& rootDirectory,
        const std::vector<std::string>& allowedExtensions)
    {
        if (rootDirectory.empty())
        {
            return;
        }

        m_Title = std::move(title);
        m_RootDirectory = rootDirectory;
        m_AllowedExtensions.clear();
        m_AllowedExtensions.reserve(allowedExtensions.size());
        for (const std::string& extension : allowedExtensions)
        {
            m_AllowedExtensions.push_back(ToLower(extension));
        }

        m_CurrentDirectory = m_RootDirectory;
        m_BackHistory.clear();
        m_GridSelectedPath.clear();
        m_Open = true;
    }

    void AssetPickerPopup::NavigateTo(const std::filesystem::path& directory)
    {
        if (m_CurrentDirectory == directory)
        {
            return;
        }

        m_BackHistory.push_back(m_CurrentDirectory);
        m_CurrentDirectory = directory;
        m_GridSelectedPath.clear();
    }

    void AssetPickerPopup::NavigateBack()
    {
        if (m_BackHistory.empty())
        {
            return;
        }

        m_CurrentDirectory = m_BackHistory.back();
        m_BackHistory.pop_back();
        m_GridSelectedPath.clear();
    }

    void AssetPickerPopup::NavigateUp()
    {
        if (m_CurrentDirectory == m_RootDirectory)
        {
            return;
        }

        NavigateTo(m_CurrentDirectory.parent_path());
    }

    void AssetPickerPopup::DrawBreadcrumb()
    {
        if (m_CurrentDirectory == m_RootDirectory)
        {
            ImGui::TextUnformatted(m_RootDirectory.filename().string().c_str());
            return;
        }

        ImGui::TextUnformatted(m_RootDirectory.filename().string().c_str());

        const std::filesystem::path relativePath = m_CurrentDirectory.lexically_relative(m_RootDirectory);
        std::filesystem::path accumulatedPath = m_RootDirectory;
        const size_t componentCount = static_cast<size_t>(std::distance(relativePath.begin(), relativePath.end()));

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

    bool AssetPickerPopup::Draw(std::filesystem::path& selectedPath)
    {
        if (!m_Open)
        {
            return false;
        }

        ImGui::OpenPopup(m_Title.c_str());
        ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Appearing);

        bool result = false;
        if (!ImGui::BeginPopupModal(m_Title.c_str(), &m_Open, ImGuiWindowFlags_NoSavedSettings))
        {
            // The popup closed itself (Cancel, the close button, or Escape) since the last frame.
            if (m_WasOpen)
            {
                m_Cancelled = true;
            }
            m_WasOpen = m_Open;
            return false;
        }

        m_WasOpen = true;

        if (!FileSystem::IsDirectory(m_CurrentDirectory))
        {
            m_CurrentDirectory = m_RootDirectory;
            m_BackHistory.clear();
            m_GridSelectedPath.clear();
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const float cancelButtonWidth = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.0f;

        if (DrawToolbarButton(BackGlyph, "Back", !m_BackHistory.empty()))
        {
            NavigateBack();
        }
        ImGui::SameLine();
        if (DrawToolbarButton(UpGlyph, "Up", m_CurrentDirectory != m_RootDirectory))
        {
            NavigateUp();
        }

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - cancelButtonWidth - style.WindowPadding.x);
        if (ImGui::Button("Cancel", ImVec2(cancelButtonWidth, 0.0f)))
        {
            m_Open = false;
            ImGui::CloseCurrentPopup();
        }

        DrawBreadcrumb();
        ImGui::Separator();

        const std::vector<std::filesystem::path> directories = FileSystem::GetDirectories(m_CurrentDirectory);
        std::vector<std::filesystem::path> files;
        for (const auto& file : FileSystem::GetFiles(m_CurrentDirectory))
        {
            if (HasAllowedExtension(file, m_AllowedExtensions))
            {
                files.push_back(file);
            }
        }

        std::filesystem::path activatedPath;
        AssetBrowserGrid::Callbacks callbacks;
        callbacks.OnActivate = [&activatedPath](const std::filesystem::path& path) { activatedPath = path; };
        AssetBrowserGrid::Draw(directories, files, m_GridSelectedPath, callbacks);

        if (!activatedPath.empty())
        {
            if (FileSystem::IsDirectory(activatedPath))
            {
                NavigateTo(activatedPath);
            }
            else
            {
                selectedPath = activatedPath;
                m_SelectedPath = activatedPath;
                result = true;
                m_Open = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
        return result;
    }
}
