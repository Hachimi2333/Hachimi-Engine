#include "Components/ComponentDrawers.h"

#include "Asset/AssetManager.h"
#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/ScriptComponent.h"
#include "UI/AssetBrowserGrid.h"
#include "UI/AssetPickerPopup.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        bool IsLuaScriptPath(const std::string& path)
        {
            std::string extension = std::filesystem::path(path).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return extension == ".lua";
        }

        // Scripts are stored relative to Assets/Scripts so a project can be moved or packaged
        // without rewriting the paths.
        void MakeScriptPathRelative(const std::string& selectedPath, ScriptComponent::ScriptReference& reference)
        {
            const std::filesystem::path scriptsDirectory = AssetManager::GetAssetsDirectory() / "Scripts";

            std::error_code errorCode;
            const std::filesystem::path relativePath = std::filesystem::relative(selectedPath, scriptsDirectory, errorCode);
            if (errorCode)
            {
                reference.Path = std::filesystem::path(selectedPath).filename().string();
                return;
            }

            reference.Path = relativePath.generic_string();
        }
    }

    void DrawScriptComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<ScriptComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<ScriptComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            context.AssetPicker.Close();
            return;
        }

        auto& script = entity.GetComponent<ScriptComponent>();

        int removeSlot = -1;
        for (int slotIndex = 0; slotIndex < static_cast<int>(script.Scripts.size()); ++slotIndex)
        {
            ScriptComponent::ScriptReference& reference = script.Scripts[slotIndex];

            ImGui::PushID(slotIndex);
            if (BeginInspectorTable("InspectorScriptRows"))
            {
                BeginInspectorProperty("Enabled");
                ImGui::Checkbox("##Enabled", &reference.Enabled);

                BeginInspectorPropertyLabel("Path");

                const float buttonWidth = ImGui::GetFrameHeight();
                const float inputWidth = std::max(ImGui::GetContentRegionAvail().x - buttonWidth * 2.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f, 40.0f);
                ImGui::SetNextItemWidth(inputWidth);

                char pathBuffer[256] = {};
                std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", reference.Path.c_str());
                if (ImGui::InputText("##Path", pathBuffer, sizeof(pathBuffer)))
                {
                    reference.Path = pathBuffer;
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Drag a Lua script here from the Content Browser");
                }

                // Accept script files dragged from the Content Browser grid.
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserGrid::FilePayload))
                    {
                        const std::string droppedPath(static_cast<const char*>(payload->Data));
                        if (IsLuaScriptPath(droppedPath))
                        {
                            MakeScriptPathRelative(droppedPath, reference);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("...", ImVec2 { buttonWidth, buttonWidth }))
                {
                    context.PendingAssetPickerSlot = slotIndex;
                    context.AssetPicker.Open("Select Script", AssetManager::GetAssetsDirectory() / "Scripts", { ".lua" });
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Browse script");
                }

                ImGui::SameLine();
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
        }

        std::filesystem::path selectedPath;
        if (context.AssetPicker.Draw(selectedPath))
        {
            const int slot = context.PendingAssetPickerSlot;
            if (slot >= 0 && slot < static_cast<int>(script.Scripts.size()))
            {
                MakeScriptPathRelative(selectedPath.string(), script.Scripts[static_cast<size_t>(slot)]);
            }
            context.PendingAssetPickerSlot = -1;
        }

        if (removeSlot >= 0)
        {
            script.Scripts.erase(script.Scripts.begin() + removeSlot);

            if (context.PendingAssetPickerSlot == removeSlot)
            {
                context.PendingAssetPickerSlot = -1;
                context.AssetPicker.Close();
            }
        }
    }
}
