#include "UI/AssetField.h"

#include "Asset/AssetDatabase.h"
#include "Components/InspectorRegistry.h"
#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "ImGui/ThemeConfig.h"
#include "Panels/EditorContext.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "UI/AssetBrowserGrid.h"

#include <imgui.h>

#include <algorithm>
#include <utility>

namespace HachimiEngine
{
    bool ApplyAssetToEntity(EditorContext& context, AssetFieldSlot slot, AssetHandle handle)
    {
        if (!slot.IsValid() || context.ActiveScene == nullptr)
        {
            return false;
        }

        Entity entity = context.ActiveScene->GetEntityByUUID(slot.Entity);
        if (!entity)
        {
            return false;
        }

        if (slot.Slot == AssetFieldMaterialSlot && entity.HasComponent<MeshRendererComponent>())
        {
            // An invalid handle clears the reference; the command covers both directions.
            if (Scope<EditorCommand> command = SceneCommands::MakeSetMeshMaterial(entity, handle))
            {
                command->Apply(*context.ActiveScene);
                RecordEdit(context.History, std::move(command));
                return true;
            }
            return false;
        }

        if (slot.Slot >= AssetFieldScriptSlotBase && entity.HasComponent<ScriptComponent>())
        {
            const uint32_t scriptIndex = slot.Slot - AssetFieldScriptSlotBase;
            auto& scripts = entity.GetComponent<ScriptComponent>().Scripts;
            if (scriptIndex >= scripts.size())
            {
                return false;
            }

            const std::string displayName = handle.IsValid() && context.Assets != nullptr
                ? context.Assets->GetDisplayName(handle)
                : std::string();

            if (Scope<EditorCommand> command =
                    SceneCommands::MakeSetScriptReference(entity, scriptIndex, handle, displayName))
            {
                command->Apply(*context.ActiveScene);
                RecordEdit(context.History, std::move(command));
                return true;
            }
        }

        return false;
    }

    std::filesystem::path GetPickerRootFor(const AssetDatabase* database, AssetType type)
    {
        if (database == nullptr)
        {
            return {};
        }

        const std::filesystem::path assets = database->GetAssetsDirectory();
        switch (type)
        {
            case AssetType::Material: return assets / "Materials";
            case AssetType::Texture: return assets / "Textures";
            case AssetType::Script: return assets / "Scripts";
            case AssetType::Scene: return assets / "Scenes";
            default: return assets;
        }
    }

    std::string GetExtensionFor(AssetType type)
    {
        switch (type)
        {
            case AssetType::Material: return ".hmaterial";
            case AssetType::Texture: return ".png";
            case AssetType::Script: return ".lua";
            case AssetType::Scene: return ".hscene";
            default: return {};
        }
    }

    void DrawAssetField(const char* id, Entity entity, uint32_t slot, AssetHandle& handle, AssetType expectedType,
                        AssetFieldState& state, InspectorDrawContext& context)
    {
        AssetDatabase* database = context.Context.Assets;

        ImGui::PushID(id);

        const bool exists = database != nullptr && database->Contains(handle);
        const std::string label = exists
            ? database->GetDisplayName(handle)
            : (handle.IsValid() ? "<Missing>" : "None");

        // The whole row is one drop target, so a dragged asset can be released on the name as well
        // as on the empty space after it.
        const float buttonWidth = ImGui::GetFrameHeight();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float textWidth = std::max(ImGui::GetContentRegionAvail().x - buttonWidth * 2.0f - spacing * 2.0f, 40.0f);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowWidth = std::max(ImGui::GetContentRegionAvail().x, textWidth);
        const float rowHeight = ImGui::GetFrameHeight();

        if (handle.IsValid() && !exists)
        {
            // A broken reference is stated in the error colour, which the theme owns.
            const ThemeConfig::SemanticColors& colors = ThemeConfig::GetColors();
            ImGui::TextColored(colors.ErrorText, "%s", label.c_str());
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("This asset is not in the project. The reference is kept, so it "
                                  "starts working again if the asset comes back.");
            }
        }
        else if (handle.IsValid())
        {
            ImGui::TextUnformatted(label.c_str());
        }
        else
        {
            ImGui::TextDisabled("None");
        }

        ImGui::SameLine();
        if (ImGui::Button("...", ImVec2(buttonWidth, buttonWidth)))
        {
            state.Pending.Entity = entity.GetUUID();
            state.Pending.Slot = slot;
            state.Picker.Open("Select Asset", GetPickerRootFor(database, expectedType),
                { GetExtensionFor(expectedType) });
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Browse for an asset");
        }

        ImGui::SameLine();
        if (DrawRemoveButton("Clear the reference"))
        {
            ApplyAssetToEntity(context.Context, AssetFieldSlot { entity.GetUUID(), slot }, AssetHandle::Invalid());
        }

        // An invisible hit area covering the rest of the row accepts the drop.
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y));
        ImGui::InvisibleButton("##AssetFieldDropZone", ImVec2(rowWidth, rowHeight));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserGrid::FilePayload))
            {
                const std::string droppedPath(static_cast<const char*>(payload->Data));
                const std::optional<AssetHandle> dropped = database != nullptr
                    ? database->GetHandleForPath(droppedPath)
                    : std::nullopt;

                if (dropped.has_value() && dropped->Type == expectedType)
                {
                    ApplyAssetToEntity(context.Context, AssetFieldSlot { entity.GetUUID(), slot }, *dropped);
                }
                else
                {
                    HE_CLIENT_WARN("'{}' is not a {} asset", droppedPath, GetExtensionFor(expectedType));
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }
}
