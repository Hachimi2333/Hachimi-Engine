#include "Components/InspectorWidgets.h"

#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "ImGui/ThemeConfig.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>

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

    bool DrawComponentHeaderUndoable(Entity entity, const ComponentDescriptor& descriptor, bool defaultOpen,
                                     bool& removed, CommandHistory* history)
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
                if (history != nullptr && entity)
                {
                    // The command snapshots the entity before the component is gone, which is what
                    // makes the removal reversible with its values intact.
                    RecordEdit(history, SceneCommands::MakeRemoveComponent(
                        *entity.GetScene(), entity, descriptor.TypeID));
                }
                else
                {
                    descriptor.Remove(entity.GetRegistry(), entity.GetHandle());
                }
                removed = true;
            }
            ImGui::PopID();
        }

        return open;
    }

    void RecordEdit(CommandHistory* history, Scope<EditorCommand> command)
    {
        if (history == nullptr || command == nullptr)
        {
            return;
        }

        // Merged rather than executed: a slider drag or a typed name reports an edit every frame,
        // and the command folds them into the one entry the user expects to undo.
        history->ExecuteMerged(std::move(command));
    }

    bool DrawStringUndoable(const char* id, Entity entity, std::string& value, CommandHistory* history, int flags)
    {
        char buffer[256] = {};
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());

        // The old text has to be copied before the widget runs: afterwards the buffer already holds
        // what the user typed and the previous value is gone.
        const std::string before = value;

        if (!ImGui::InputText(id, buffer, sizeof(buffer), flags))
        {
            return false;
        }

        value = buffer;
        if (history != nullptr && entity)
        {
            RecordEdit(history, SceneCommands::MakeSetTag(entity, before, value));
        }

        return true;
    }
}
