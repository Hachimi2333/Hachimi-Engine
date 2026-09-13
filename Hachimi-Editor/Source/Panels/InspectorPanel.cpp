#include "Panels/InspectorPanel.h"

#include "Asset/AssetDatabase.h"
#include "Asset/MaterialAsset.h"
#include "Asset/TextureCache.h"
#include "Components/InspectorRegistry.h"
#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Editor/SceneDirtyState.h"
#include "ImGui/ThemeConfig.h"
#include "Panels/EditorContext.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "UI/AssetBrowserGrid.h"
#include "Utils/FileSystem.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

namespace HachimiEngine
{
    namespace
    {
        // Column weights keep every property label left-aligned and every control starting at the
        // same x.
        constexpr float InspectorLabelColumnWeight = 0.45f;
        constexpr float InspectorControlColumnWeight = 0.55f;

        // Slot the material document's texture field occupies. A material field records the asset
        // being edited in place of an entity, so it can never collide with a component field.
        constexpr uint32_t MaterialAssetTextureSlot = 0;

        // Popup identifier shared by the button that opens it and the popup that draws the
        // entries, so the two cannot end up naming different slots of the ID stack.
        constexpr const char* AddComponentPopupId = "AddComponentPopup";

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

        // The picker is drawn first and its answer applied before the rows that consume it, so a
        // choice is reflected in the same frame the modal closes.
        std::filesystem::path pickedPath;
        if (m_AssetFields.Picker.Draw(pickedPath))
        {
            const AssetFieldSlot slot = m_AssetFields.Pending;
            m_AssetFields.Pending.Clear();

            if (slot.IsValid() && context.Assets != nullptr)
            {
                const std::optional<AssetHandle> picked = context.Assets->GetHandleForPath(pickedPath);
                if (!picked.has_value())
                {
                    HE_CLIENT_ERROR("Selected file is not a project asset: {}", pickedPath.string());
                }
                else if (context.SelectedAsset.IsValid() && slot.Entity == context.SelectedAsset.ID)
                {
                    // A material document addresses its own fields by the asset being edited; its
                    // editor takes the handle from the picker directly.
                    m_MaterialBuffer.AlbedoTexture = *picked;
                    m_MaterialChanged = true;
                }
                else
                {
                    ApplyAssetToEntity(context, slot, *picked);
                }
            }
        }
        else if (m_AssetFields.Picker.ConsumeCancelled())
        {
            // A cancelled pick applies nothing, so the slot it was opened for must not stay armed:
            // the next confirmed pick would otherwise be written into the wrong field.
            m_AssetFields.Pending.Clear();
        }

        if (context.SelectedAsset.IsValid() && context.Assets != nullptr)
        {
            DrawAssetInspector(context);
        }
        else if (!context.SelectedEntity || context.ActiveScene == nullptr)
        {
            m_AssetFields.Picker.Close();
            m_AssetFields.Pending.Clear();
            ImGui::TextDisabled("No entity or asset selected");
        }
        else
        {
            DrawEntityInspector(context, context.SelectedEntity);
        }

        ImGui::End();
    }

    void InspectorPanel::DrawEntityInspector(EditorContext& context, Entity entity)
    {
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
                // Every keystroke is one command, merged into the single "Rename Entity" the user
                // expects to undo.
                const std::string before = entity.GetName();
                entity.GetComponent<TagComponent>().Tag = tagBuffer;
                RecordEdit(context.History, SceneCommands::MakeSetTag(entity, before, entity.GetName()));
            }
            ImGui::EndTable();
        }

        ImGui::TextDisabled("UUID: %s", entity.GetUUID().ToString().c_str());
        ImGui::Separator();

        InspectorDrawContext drawContext { context, m_AssetFields };

        // The component list comes from the engine registry, in registration order, so a newly
        // registered component appears here without touching this panel.
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (IsDrawnByThePanelHeader(descriptor.TypeID)
                || !descriptor.Has(entity.GetRegistry(), entity.GetHandle()))
            {
                continue;
            }

            // Each drawer numbers its own asset fields from zero, so two components cannot claim
            // the same slot by accident.
            drawContext.NextAssetFieldSlot = 0;

            const ComponentDrawFn drawer = InspectorRegistry::Find(descriptor.TypeID);
            if (drawer != nullptr)
            {
                drawer(entity, drawContext);
                continue;
            }

            // Registered but not editable yet: still show the header, so a component the engine
            // understands is never invisible in the editor.
            bool removed = false;
            if (DrawComponentHeaderUndoable(entity, descriptor, false, removed, context.History))
            {
                ImGui::TextDisabled("No editor for this component yet");
            }
        }

        ImGui::Separator();

        // The two entity-level actions sit together at the bottom, below the component list.
        DrawAddComponentMenu(context, entity);

        if (ImGui::Button("Delete Entity", GetFullWidthButtonSize()))
        {
            if (context.History != nullptr)
            {
                // The whole subtree is recorded before the delete, so undo brings back the parent
                // and its children rather than only the parent.
                RecordEdit(context.History, SceneCommands::MakeDestroyEntity(*context.ActiveScene, entity));
            }
            else
            {
                context.ActiveScene->DestroyEntity(entity);
            }

            context.SelectEntity({});
        }
    }

    void InspectorPanel::DrawAddComponentMenu(EditorContext& context, Entity entity)
    {
        if (ImGui::Button("Add Component", GetFullWidthButtonSize()))
        {
            ImGui::OpenPopup(AddComponentPopupId);
        }

        // The popup is submitted on every frame it is open, not only on the frame the button was
        // clicked: BeginPopup() is what builds the popup's window, so a skipped frame is a frame
        // without a menu. The opening frame alone cannot be used either, because an auto-resized
        // window is still sized from the previous frame's content and would show up empty.
        if (!ImGui::BeginPopup(AddComponentPopupId))
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
                // The command applies through the descriptor, so a component added here is
                // removable by undo without this menu knowing what it is.
                if (Scope<EditorCommand> command = SceneCommands::MakeAddComponent(entity, descriptor.TypeID))
                {
                    command->Apply(*context.ActiveScene);
                    RecordEdit(context.History, std::move(command));
                }
            }

            for (const ComponentAddPreset& preset : descriptor.Presets)
            {
                const std::string presetLabel(preset.Label);
                if (ImGui::MenuItem(presetLabel.c_str()))
                {
                    if (preset.Add != nullptr)
                    {
                        preset.Add(entity.GetRegistry(), entity.GetHandle());
                        if (context.DirtyState != nullptr)
                        {
                            context.DirtyState->MarkDirty();
                        }
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    void InspectorPanel::DrawAssetInspector(EditorContext& context)
    {
        AssetDatabase& assets = *context.Assets;
        const AssetMeta* meta = assets.GetMeta(context.SelectedAsset);

        if (ImGui::BeginTable("InspectorAssetHeader", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##AssetHeaderLabel", ImGuiTableColumnFlags_WidthStretch, InspectorLabelColumnWeight);
            ImGui::TableSetupColumn("##AssetHeaderValue", ImGuiTableColumnFlags_WidthStretch, InspectorControlColumnWeight);

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Asset");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(assets.GetDisplayName(context.SelectedAsset).c_str());

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Path");
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", assets.GetAssetPath(context.SelectedAsset).generic_string().c_str());

            ImGui::EndTable();
        }

        ImGui::Separator();

        if (meta == nullptr)
        {
            const ThemeConfig::SemanticColors& colors = ThemeConfig::GetColors();
            ImGui::TextColored(colors.ErrorText, "This asset is no longer in the project");
        }
        else
        {
            switch (meta->Type)
            {
                case AssetType::Material: DrawMaterialAssetEditor(context); break;
                case AssetType::Texture: DrawTextureAssetEditor(context); break;
                case AssetType::Scene:
                case AssetType::Script:
                case AssetType::Project:
                default:
                    ImGui::TextDisabled("This asset type has no settings yet");
                    break;
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Deselect", GetFullWidthButtonSize()))
        {
            context.SelectAsset(AssetHandle::Invalid(), {});
        }
    }

    void InspectorPanel::EnsureMaterialBuffer(AssetDatabase& assets, AssetHandle handle)
    {
        if (m_MaterialBufferValid && m_MaterialBufferFor == handle)
        {
            return;
        }

        std::string text;
        MaterialAsset material;
        m_MaterialBufferValid = assets.ReadAssetText(handle, text) && MaterialAsset::Deserialize(text, material);
        if (m_MaterialBufferValid)
        {
            m_MaterialBuffer = material;
        }

        m_MaterialBufferFor = handle;
        m_MaterialChanged = false;
    }

    void InspectorPanel::DrawMaterialTextureRow(EditorContext& context, MaterialAsset& edited)
    {
        AssetDatabase& assets = *context.Assets;

        const bool exists = assets.Contains(edited.AlbedoTexture);
        if (exists)
        {
            ImGui::TextUnformatted(assets.GetDisplayName(edited.AlbedoTexture).c_str());
        }
        else if (edited.AlbedoTexture.IsValid())
        {
            const ThemeConfig::SemanticColors& colors = ThemeConfig::GetColors();
            ImGui::TextColored(colors.ErrorText, "<Missing>");
        }
        else
        {
            ImGui::TextDisabled("None");
        }

        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const float rowHeight = ImGui::GetFrameHeight();

        ImGui::SameLine();
        if (ImGui::Button("...", ImVec2(rowHeight, rowHeight)))
        {
            OpenMaterialTexturePicker(context);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Browse for a texture");
        }

        ImGui::SameLine();
        if (DrawRemoveButton("Clear the texture"))
        {
            edited.AlbedoTexture = AssetHandle::Invalid();
            m_MaterialChanged = true;
        }

        // A texture dropped from the content browser lands here like it does on a component field.
        ImGui::SetCursorScreenPos(rowMin);
        ImGui::InvisibleButton("##MaterialTextureDrop", ImVec2(std::max(ImGui::GetContentRegionAvail().x, 1.0f), rowHeight));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserGrid::FilePayload))
            {
                const std::string droppedPath(static_cast<const char*>(payload->Data));
                const std::optional<AssetHandle> dropped = assets.GetHandleForPath(droppedPath);
                if (dropped.has_value() && dropped->Type == AssetType::Texture)
                {
                    edited.AlbedoTexture = *dropped;
                    m_MaterialChanged = true;
                }
                else
                {
                    HE_CLIENT_WARN("'{}' is not a texture asset", droppedPath);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void InspectorPanel::OpenMaterialTexturePicker(EditorContext& context)
    {
        // The material document records the asset being edited in place of an entity, which is what
        // tells the panel's selection handler that no entity is involved.
        m_AssetFields.Pending.Entity = context.SelectedAsset.ID;
        m_AssetFields.Pending.Slot = MaterialAssetTextureSlot;
        m_AssetFields.Picker.Open("Select Texture",
            GetPickerRootFor(context.Assets, AssetType::Texture),
            { GetExtensionFor(AssetType::Texture) });
    }

    void InspectorPanel::DrawMaterialAssetEditor(EditorContext& context)
    {
        AssetDatabase& assets = *context.Assets;
        EnsureMaterialBuffer(assets, context.SelectedAsset);

        if (!m_MaterialBufferValid)
        {
            const ThemeConfig::SemanticColors& colors = ThemeConfig::GetColors();
            ImGui::TextColored(colors.ErrorText, "This material cannot be read");
            return;
        }

        if (BeginEntityTable("InspectorMaterialRows"))
        {
            // A material may only name an engine shader, and the engine owns a fixed set of them, so
            // a combo beats a text field that can silently name a program that does not exist.
            static const char* ShaderNames[] = { "Default.glsl" };
            int shaderIndex = 0;
            for (int index = 0; index < IM_ARRAYSIZE(ShaderNames); ++index)
            {
                if (m_MaterialBuffer.Shader == ShaderNames[index])
                {
                    shaderIndex = index;
                }
            }

            BeginInspectorProperty("Shader");
            if (ImGui::Combo("##Shader", &shaderIndex, ShaderNames, IM_ARRAYSIZE(ShaderNames)))
            {
                m_MaterialBuffer.Shader = ShaderNames[shaderIndex];
                m_MaterialChanged = true;
            }

            BeginInspectorProperty("Base Color");
            m_MaterialChanged |= ImGui::ColorEdit4("##BaseColor", Math::ValuePtr(m_MaterialBuffer.BaseColor));

            BeginInspectorProperty("Albedo Texture");
            DrawMaterialTextureRow(context, m_MaterialBuffer);

            BeginInspectorProperty("Roughness");
            m_MaterialChanged |= ImGui::SliderFloat("##MaterialRoughness", &m_MaterialBuffer.Roughness, 0.0f, 1.0f);

            BeginInspectorProperty("Metallic");
            m_MaterialChanged |= ImGui::SliderFloat("##MaterialMetallic", &m_MaterialBuffer.Metallic, 0.0f, 1.0f);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::BeginDisabled(!m_MaterialChanged);
        if (ImGui::Button("Apply", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0.0f)))
        {
            const std::filesystem::path assetPath = assets.GetAssetPath(context.SelectedAsset);
            if (!FileSystem::WriteTextFile(assetPath, MaterialAsset::Serialize(m_MaterialBuffer)))
            {
                HE_CLIENT_ERROR("Could not write the material {}", assetPath.string());
            }
            else
            {
                // Applying is a write the database did not make itself, so it is told about it:
                // that bump is what makes the renderer rebuild the material it had cached.
                assets.NotifyAssetChanged(context.SelectedAsset);
                HE_CLIENT_INFO("Saved material {}", assetPath.generic_string());
                m_MaterialChanged = false;

                if (context.Textures != nullptr && m_MaterialBuffer.AlbedoTexture.IsValid())
                {
                    context.Textures->Evict(m_MaterialBuffer.AlbedoTexture);
                }

                if (context.DirtyState != nullptr)
                {
                    context.DirtyState->MarkDirty();
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Revert", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            // Re-reading the file is what reverting means for a document the panel buffers itself.
            m_MaterialBufferValid = false;
            EnsureMaterialBuffer(assets, context.SelectedAsset);
        }

        if (assets.IsReadOnly())
        {
            ImGui::TextDisabled("The content root is a read-only game package");
        }
    }

    void InspectorPanel::DrawTextureAssetEditor(EditorContext& context)
    {
        AssetDatabase& assets = *context.Assets;
        const AssetMeta* meta = assets.GetMeta(context.SelectedAsset);
        if (meta == nullptr)
        {
            return;
        }

        AssetMeta edited = *meta;
        bool changed = false;

        if (BeginEntityTable("InspectorTextureRows"))
        {
            static const char* TypeNames[] = { "Color", "Normal", "Data" };
            int type = static_cast<int>(edited.Texture.Type);
            BeginInspectorProperty("Type");
            if (ImGui::Combo("##TextureType", &type, TypeNames, IM_ARRAYSIZE(TypeNames)))
            {
                edited.Texture.Type = static_cast<TextureType>(type);
                changed = true;
            }

            static const char* WrapNames[] = { "Repeat", "Clamp", "Mirrored Repeat" };
            int wrap = static_cast<int>(edited.Texture.Wrap);
            BeginInspectorProperty("Wrap");
            if (ImGui::Combo("##TextureWrap", &wrap, WrapNames, IM_ARRAYSIZE(WrapNames)))
            {
                edited.Texture.Wrap = static_cast<TextureWrapMode>(wrap);
                changed = true;
            }

            static const char* FilterNames[] = { "Nearest", "Linear" };
            int filter = static_cast<int>(edited.Texture.Filter);
            BeginInspectorProperty("Filter");
            if (ImGui::Combo("##TextureFilter", &filter, FilterNames, IM_ARRAYSIZE(FilterNames)))
            {
                edited.Texture.Filter = static_cast<TextureFilterMode>(filter);
                changed = true;
            }

            BeginInspectorProperty("Mipmaps");
            changed |= ImGui::Checkbox("##TextureMipmaps", &edited.Texture.GenerateMipmaps);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::BeginDisabled(!changed);
        if (ImGui::Button("Apply", GetFullWidthButtonSize()))
        {
            if (assets.WriteMeta(context.SelectedAsset, edited) != AssetWriteResult::Success)
            {
                HE_CLIENT_ERROR("Could not write the texture settings");
            }
            else if (context.Textures != nullptr)
            {
                // The new settings only reach the GPU through a fresh upload.
                context.Textures->Evict(context.SelectedAsset);
            }
        }
        ImGui::EndDisabled();
    }
}
