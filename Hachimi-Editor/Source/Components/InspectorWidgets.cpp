#include "Components/InspectorWidgets.h"

#include "ImGui/ThemeConfig.h"

#include <algorithm>
#include <cfloat>

namespace HachimiEngine
{
    bool DrawRemoveButton(const char* tooltip)
    {
        const ImVec2 buttonSize { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() };

        // The destructive-button palette belongs to the theme, not to this widget.
        const ThemeConfig::SemanticColors& colors = ThemeConfig::GetColors();
        ImGui::PushStyleColor(ImGuiCol_Button, colors.DestructiveButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors.DestructiveButtonHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors.DestructiveButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text, colors.DestructiveButtonText);
        const bool clicked = ImGui::Button("X", buttonSize);
        ImGui::PopStyleColor(4);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        return clicked;
    }

    bool BeginInspectorTable(const char* id)
    {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
        {
            return false;
        }

        // Column weights keep every property label left-aligned and every control starting at
        // the same x.
        ImGui::TableSetupColumn("##InspectorLabel", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("##InspectorControl", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        return true;
    }

    void BeginInspectorProperty(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void BeginInspectorPropertyLabel(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
    }

    ImVec2 GetFullWidthButtonSize()
    {
        return { ImGui::GetContentRegionAvail().x, 0.0f };
    }

    bool DrawComponentHeader(Entity entity, const ComponentDescriptor& descriptor, bool defaultOpen, bool& removed)
    {
        const std::string label(descriptor.DisplayName);
        const ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        const bool open = ImGui::CollapsingHeader(label.c_str(), flags);

        if (!descriptor.Required)
        {
            const float buttonWidth = ImGui::GetFrameHeight();
            ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - buttonWidth, 0.0f));

            ImGui::PushID(label.c_str());
            if (DrawRemoveButton("Remove component"))
            {
                descriptor.Remove(entity.GetRegistry(), entity.GetHandle());
                removed = true;
            }
            ImGui::PopID();
        }

        return open;
    }
}
