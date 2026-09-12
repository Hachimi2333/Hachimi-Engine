#include "Panels/InspectorPanel.h"

#include "Components/InspectorRegistry.h"
#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Panels/EditorContext.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace HachimiEngine
{
    namespace
    {
        // Column weights keep every property label left-aligned and every control starting at the
        // same x.
        constexpr float InspectorLabelColumnWeight = 0.45f;
        constexpr float InspectorControlColumnWeight = 0.55f;

        bool BeginEntityTable(const char* id)
        {
            if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
            {
                return false;
            }

            ImGui::TableSetupColumn("##InspectorLabel", ImGuiTableColumnFlags_WidthStretch, InspectorLabelColumnWeight);
            ImGui::TableSetupColumn("##InspectorControl", ImGuiTableColumnFlags_WidthStretch, InspectorControlColumnWeight);
            return true;
        }

        // Identity and name are drawn directly at the top of the panel rather than as a
        // collapsible section, so the component list leaves them out.
        bool IsDrawnByThePanelHeader(entt::id_type typeId)
        {
            return typeId == entt::type_hash<IDComponent>::value()
                || typeId == entt::type_hash<TagComponent>::value();
        }
    }

    void InspectorPanel::Draw(EditorContext& context)
    {
        if (!ImGui::Begin("Inspector"))
        {
            ImGui::End();
            return;
        }

        if (!context.SelectedEntity || context.ActiveScene == nullptr)
        {
            m_AssetPicker.Close();
            m_PendingAssetPickerSlot = -1;
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        Entity entity = context.SelectedEntity;
        char tagBuffer[128] = {};
        std::snprintf(tagBuffer, sizeof(tagBuffer), "%s", entity.GetName().c_str());

        if (BeginEntityTable("InspectorEntityRows"))
        {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Tag");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);

            if (ImGui::InputText("##Tag", tagBuffer, sizeof(tagBuffer)))
            {
                entity.GetComponent<TagComponent>().Tag = tagBuffer;
            }
            ImGui::EndTable();
        }

        ImGui::TextDisabled("UUID: %s", entity.GetUUID().ToString().c_str());
        ImGui::Separator();

        InspectorDrawContext drawContext { context, m_AssetPicker, m_PendingAssetPickerSlot };

        // The component list comes from the engine registry, in registration order, so a newly
        // registered component appears here without touching this panel.
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (IsDrawnByThePanelHeader(descriptor.TypeID)
                || !descriptor.Has(entity.GetRegistry(), entity.GetHandle()))
            {
                continue;
            }

            const ComponentDrawFn drawer = InspectorRegistry::Find(descriptor.TypeID);
            if (drawer != nullptr)
            {
                drawer(entity, drawContext);
                continue;
            }

            // Registered but not editable yet: still show the header, so a component the engine
            // understands is never invisible in the editor.
            bool removed = false;
            if (DrawComponentHeader(entity, descriptor, false, removed))
            {
                ImGui::TextDisabled("No editor for this component yet");
            }
        }

        ImGui::Separator();
        DrawAddComponentMenu(entity);

        if (ImGui::Button("Delete Entity", GetFullWidthButtonSize()))
        {
            context.ActiveScene->DestroyEntity(entity);
            context.SelectedEntity = {};
        }

        ImGui::End();
    }

    void InspectorPanel::DrawAddComponentMenu(Entity entity)
    {
        if (!ImGui::Button("Add Component", GetFullWidthButtonSize()))
        {
            return;
        }

        ImGui::OpenPopup("AddComponentPopup");
        if (!ImGui::BeginPopup("AddComponentPopup"))
        {
            return;
        }

        // One entry per registered component the entity does not have, plus the extra presets a
        // descriptor offers (one collider shape per entry).
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.Required || descriptor.Has(entity.GetRegistry(), entity.GetHandle()))
            {
                continue;
            }

            const std::string label(descriptor.DisplayName);
            const std::string itemId = label + " Component";
            if (ImGui::MenuItem(itemId.c_str()))
            {
                descriptor.AddDefault(entity.GetRegistry(), entity.GetHandle());
            }

            for (const ComponentAddPreset& preset : descriptor.Presets)
            {
                const std::string presetLabel(preset.Label);
                if (ImGui::MenuItem(presetLabel.c_str()))
                {
                    preset.Add(entity.GetRegistry(), entity.GetHandle());
                }
            }
        }

        ImGui::EndPopup();
    }
}
