#include "Panels/SceneHierarchyPanel.h"

#include "Components/InspectorWidgets.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Scene.h"

#include <imgui.h>

namespace HachimiEngine
{
    void SceneHierarchyPanel::Draw(EditorContext& context)
    {
        if (!ImGui::Begin("Scene Hierarchy"))
        {
            ImGui::End();
            return;
        }

        if (context.ActiveScene == nullptr)
        {
            ImGui::TextDisabled("No active scene");
            ImGui::End();
            return;
        }

        if (ImGui::BeginPopupContextWindow("SceneHierarchyContext"))
        {
            DrawCreateMenu(context);
            ImGui::EndPopup();
        }

        for (const Entity entity : context.ActiveScene->GetAllEntities())
        {
            if (!entity.HasComponent<RelationshipComponent>())
            {
                continue;
            }

            const auto& relationship = entity.GetComponent<RelationshipComponent>();
            if (relationship.Parent == UUID::Invalid())
            {
                DrawEntityNode(context, entity);
            }
        }

        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(EditorContext& context, Entity entity)
    {
        if (!entity)
        {
            return;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (context.SelectedEntity == entity)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // The entity handle pointer keeps duplicate names unique without showing the UUID.
        const std::string label = entity.GetName();
        const bool expanded = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity))), flags, "%s", label.c_str());

        if (ImGui::IsItemClicked())
        {
            context.SelectEntity(entity);
        }

        DrawEntityContextMenu(context, entity);

        if (expanded)
        {
            for (const Entity child : context.ActiveScene->GetChildren(entity))
            {
                DrawEntityNode(context, child);
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawEntityContextMenu(EditorContext& context, Entity entity)
    {
        const std::string popupName = "EntityContext##" + entity.GetUUID().ToString();
        if (ImGui::BeginPopupContextItem(popupName.c_str()))
        {
            context.SelectEntity(entity);

            if (ImGui::MenuItem("Delete Entity"))
            {
                // Through the history, so both the panel and the Inspector delete the same way and
                // the delete can be undone with the whole subtree.
                if (context.History != nullptr)
                {
                    RecordEdit(context.History, SceneCommands::MakeDestroyEntity(*context.ActiveScene, entity));
                }
                else
                {
                    context.ActiveScene->DestroyEntity(entity);
                }

                if (context.SelectedEntity == entity)
                {
                    context.SelectEntity({});
                }
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Duplicate Entity"))
            {
                Entity duplicate;
                if (context.History != nullptr)
                {
                    if (Scope<EditorCommand> command =
                            SceneCommands::MakeDuplicateEntity(*context.ActiveScene, entity, duplicate))
                    {
                        RecordEdit(context.History, std::move(command));
                    }
                }
                else
                {
                    duplicate = context.ActiveScene->DuplicateEntity(entity);
                }

                if (duplicate)
                {
                    context.SelectEntity(duplicate);
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void SceneHierarchyPanel::DrawCreateMenu(EditorContext& context)
    {
        if (ImGui::MenuItem("Create Empty Entity"))
        {
            context.SelectEntity(context.ActiveScene->CreateEntity("Empty Entity"));
        }
        if (ImGui::MenuItem("Create Cube"))
        {
            context.SelectEntity(CreatePrimitive(context, "Cube", PrimitiveMeshType::Cube));
        }
        if (ImGui::MenuItem("Create Sphere"))
        {
            context.SelectEntity(CreatePrimitive(context, "Sphere", PrimitiveMeshType::Sphere));
        }
        if (ImGui::MenuItem("Create Plane"))
        {
            context.SelectEntity(CreatePrimitive(context, "Plane", PrimitiveMeshType::Plane));
        }
        if (ImGui::MenuItem("Create Point Light"))
        {
            Entity entity = context.ActiveScene->CreateEntity("Point Light");
            auto& light = entity.AddComponent<LightComponent>();
            light.Type = LightComponent::LightType::Point;
            context.SelectEntity(entity);
        }
        if (ImGui::MenuItem("Create Directional Light"))
        {
            Entity entity = context.ActiveScene->CreateEntity("Directional Light");
            auto& light = entity.AddComponent<LightComponent>();
            light.Type = LightComponent::LightType::Directional;
            light.Intensity = 1.4f;
            context.SelectEntity(entity);
        }
        if (ImGui::MenuItem("Create Camera"))
        {
            Entity entity = context.ActiveScene->CreateEntity("Camera");
            entity.AddComponent<CameraComponent>();
            context.SelectEntity(entity);
        }
    }

    Entity SceneHierarchyPanel::CreatePrimitive(EditorContext& context, const char* name, PrimitiveMeshType primitive)
    {
        Entity entity = context.ActiveScene->CreateEntity(name);
        entity.AddComponent<MeshRendererComponent>().SetPrimitive(primitive);
        return entity;
    }
}
