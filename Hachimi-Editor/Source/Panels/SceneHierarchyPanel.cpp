#include "Panels/SceneHierarchyPanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>

namespace HachimiEngine
{
    void SceneHierarchyPanel::Draw(EditorContext& context)
    {
        ImGui::Begin("Scene Hierarchy");

        if (context.Scene == nullptr)
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

        for (const Entity entity : context.Scene->GetAllEntities())
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

        const std::string label = entity.GetName() + "##" + entity.GetUUID().ToString();
        const bool expanded = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity))), flags, "%s", label.c_str());

        if (ImGui::IsItemClicked())
        {
            context.SelectedEntity = entity;
        }

        DrawEntityContextMenu(context, entity);

        if (expanded)
        {
            const auto& relationship = entity.GetComponent<RelationshipComponent>();
            for (const UUID childUUID : relationship.Children)
            {
                DrawEntityNode(context, context.Scene->GetEntityByUUID(childUUID));
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawEntityContextMenu(EditorContext& context, Entity entity)
    {
        const std::string popupName = "EntityContext##" + entity.GetUUID().ToString();
        if (ImGui::BeginPopupContextItem(popupName.c_str()))
        {
            context.SelectedEntity = entity;

            if (ImGui::MenuItem("Delete Entity"))
            {
                context.Scene->DestroyEntity(entity);
                if (context.SelectedEntity == entity)
                {
                    context.SelectedEntity = {};
                }
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Duplicate Entity"))
            {
                context.Scene->DuplicateEntity(entity);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void SceneHierarchyPanel::DrawCreateMenu(EditorContext& context)
    {
        if (ImGui::MenuItem("Create Empty Entity"))
        {
            context.Scene->CreateEntity("Empty Entity");
        }
        if (ImGui::MenuItem("Create Cube"))
        {
            Entity entity = context.Scene->CreateEntity("Cube");
            auto& mesh = entity.AddComponent<MeshComponent>();
            mesh.PrimitiveType = PrimitiveMeshType::Cube;
            mesh.Mesh = MeshFactory::CreateCube();
            context.SelectedEntity = entity;
        }
        if (ImGui::MenuItem("Create Sphere"))
        {
            Entity entity = context.Scene->CreateEntity("Sphere");
            auto& mesh = entity.AddComponent<MeshComponent>();
            mesh.PrimitiveType = PrimitiveMeshType::Sphere;
            mesh.Mesh = MeshFactory::CreateSphere();
            context.SelectedEntity = entity;
        }
        if (ImGui::MenuItem("Create Plane"))
        {
            Entity entity = context.Scene->CreateEntity("Plane");
            auto& mesh = entity.AddComponent<MeshComponent>();
            mesh.PrimitiveType = PrimitiveMeshType::Plane;
            mesh.Mesh = MeshFactory::CreatePlane();
            context.SelectedEntity = entity;
        }
        if (ImGui::MenuItem("Create Point Light"))
        {
            Entity entity = context.Scene->CreateEntity("Point Light");
            auto& light = entity.AddComponent<LightComponent>();
            light.Type = LightComponent::LightType::Point;
            context.SelectedEntity = entity;
        }
        if (ImGui::MenuItem("Create Directional Light"))
        {
            Entity entity = context.Scene->CreateEntity("Directional Light");
            auto& light = entity.AddComponent<LightComponent>();
            light.Type = LightComponent::LightType::Directional;
            light.Intensity = 1.4f;
            context.SelectedEntity = entity;
        }
        if (ImGui::MenuItem("Create Camera"))
        {
            Entity entity = context.Scene->CreateEntity("Camera");
            entity.AddComponent<CameraComponent>();
            context.SelectedEntity = entity;
        }
    }
}
