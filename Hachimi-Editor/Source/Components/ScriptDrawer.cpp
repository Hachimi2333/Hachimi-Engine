#include "Components/ComponentDrawers.h"

#include "Asset/AssetDatabase.h"
#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Editor/SceneDirtyState.h"
#include "Panels/EditorContext.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"
#include "UI/AssetField.h"

#include <imgui.h>

#include <cstdio>

namespace HachimiEngine
{
    void DrawScriptComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<ScriptComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<ScriptComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            context.AssetFields.Picker.Close();
            context.AssetFields.Pending.Clear();
            return;
        }

        auto& script = entity.GetComponent<ScriptComponent>();
        CommandHistory* history = context.Context.History;
        AssetDatabase* database = context.Context.Assets;

        int removeSlot = -1;
        for (int slotIndex = 0; slotIndex < static_cast<int>(script.Scripts.size()); ++slotIndex)
        {
            ScriptComponent::ScriptReference& reference = script.Scripts[slotIndex];

            ImGui::PushID(slotIndex);
            if (BeginInspectorTable("InspectorScriptRows"))
            {
                BeginInspectorProperty("Enabled");
                ImGui::Checkbox("##Enabled", &reference.Enabled);

                BeginInspectorProperty("Script");
                context.NextAssetFieldSlot = AssetFieldScriptSlotBase + static_cast<uint32_t>(slotIndex);
                DrawAssetField("##Script", entity, context.NextAssetFieldSlot, reference.Script,
                    AssetType::Script, context.AssetFields, context);

                // The display name is only a label for a reference whose asset is gone; when the
                // asset is known, the database already knows the better name.
                const bool known = database != nullptr && database->Contains(reference.Script);
                BeginInspectorProperty("Name");
                if (known)
                {
                    ImGui::TextDisabled("%s", database->GetDisplayName(reference.Script).c_str());
                }
                else
                {
                    char nameBuffer[256] = {};
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", reference.DisplayName.c_str());
                    if (ImGui::InputText("##DisplayName", nameBuffer, sizeof(nameBuffer)))
                    {
                        reference.DisplayName = nameBuffer;
                        if (context.Context.DirtyState != nullptr)
                        {
                            context.Context.DirtyState->MarkDirty();
                        }
                    }
                }

                BeginInspectorProperty("Remove");
                if (DrawRemoveButton("Remove script"))
                {
                    removeSlot = slotIndex;
                }

                ImGui::EndTable();
            }
            ImGui::PopID();

            ImGui::Separator();
        }

        if (ImGui::Button("Add Script", GetFullWidthButtonSize()))
        {
            script.Scripts.emplace_back();
            ImGui::Separator();
        }

        if (removeSlot >= 0)
        {
            // Dropping a slot shifts every later one, so the undo stack is dropped rather than left
            // holding commands that now address a different script. The edit itself still counts as
            // an unsaved change.
            script.Scripts.erase(script.Scripts.begin() + removeSlot);
            if (history != nullptr)
            {
                history->Clear();
            }
            if (context.Context.DirtyState != nullptr)
            {
                context.Context.DirtyState->MarkDirty();
            }
        }
    }
}
