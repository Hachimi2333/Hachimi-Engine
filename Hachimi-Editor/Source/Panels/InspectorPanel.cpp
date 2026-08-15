#include "Panels/InspectorPanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <cstdio>

namespace HachimiEngine
{
    void InspectorPanel::Draw(EditorContext& context)
    {
        ImGui::Begin("Inspector");

        if (!context.SelectedEntity || context.Scene == nullptr)
        {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        Entity entity = context.SelectedEntity;
        char tagBuffer[128] = {};
        std::snprintf(tagBuffer, sizeof(tagBuffer), "%s", entity.GetName().c_str());

        if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer)))
        {
            entity.GetComponent<TagComponent>().Tag = tagBuffer;
        }

        ImGui::Text("UUID: %s", entity.GetUUID().ToString().c_str());
        ImGui::Separator();

        DrawTransform(entity);

        if (entity.HasComponent<MeshComponent>())
        {
            DrawMesh(entity);
        }
        if (entity.HasComponent<CameraComponent>())
        {
            DrawCamera(entity);
        }
        if (entity.HasComponent<LightComponent>())
        {
            DrawLight(entity);
        }

        ImGui::Separator();
        DrawAddComponentMenu(context, entity);

        if (ImGui::Button("Delete Entity"))
        {
            context.Scene->DestroyEntity(entity);
            context.SelectedEntity = {};
        }

        ImGui::End();
    }

    void InspectorPanel::DrawAddComponentMenu(EditorContext& context, Entity entity)
    {
        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (!entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Mesh Component"))
            {
                auto& mesh = entity.AddComponent<MeshComponent>();
                mesh.PrimitiveType = PrimitiveMeshType::Cube;
                mesh.Mesh = MeshFactory::CreateCube();
            }
            if (!entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera Component"))
            {
                entity.AddComponent<CameraComponent>();
            }
            if (!entity.HasComponent<LightComponent>() && ImGui::MenuItem("Light Component"))
            {
                entity.AddComponent<LightComponent>();
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::DrawTransform(Entity entity)
    {
        if (!entity.HasComponent<TransformComponent>())
        {
            return;
        }

        auto& transform = entity.Transform();
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.05f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.25f);
            ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.05f, 0.01f, 100.0f);
        }
    }

    void InspectorPanel::DrawMesh(Entity entity)
    {
        auto& mesh = entity.GetComponent<MeshComponent>();
        if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        const char* primitiveNames[] = { "Cube", "Sphere", "Plane", "Grid" };
        int primitiveType = static_cast<int>(mesh.PrimitiveType) - static_cast<int>(PrimitiveMeshType::Cube);
        if (primitiveType < 0)
        {
            primitiveType = 0;
        }

        if (ImGui::Combo("Primitive", &primitiveType, primitiveNames, IM_ARRAYSIZE(primitiveNames)))
        {
            mesh.PrimitiveType = static_cast<PrimitiveMeshType>(primitiveType + static_cast<int>(PrimitiveMeshType::Cube));
            mesh.Mesh = MeshFactory::CreatePrimitive(mesh.PrimitiveType);
        }

        ImGui::ColorEdit4("Albedo Color", glm::value_ptr(mesh.MaterialColor));
        ImGui::SliderFloat("Roughness", &mesh.Roughness, 0.01f, 1.0f);
        ImGui::SliderFloat("Metallic", &mesh.Metallic, 0.0f, 1.0f);
        ImGui::Checkbox("Visible", &mesh.Visible);

        if (mesh.MaterialOverride != nullptr)
        {
            mesh.MaterialOverride->SetAlbedoColor(mesh.MaterialColor);
            mesh.MaterialOverride->SetRoughness(mesh.Roughness);
            mesh.MaterialOverride->SetMetallic(mesh.Metallic);
        }
    }

    void InspectorPanel::DrawCamera(Entity entity)
    {
        auto& camera = entity.GetComponent<CameraComponent>();
        if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::Checkbox("Primary", &camera.Primary);
        ImGui::SliderFloat("Field Of View", &camera.FieldOfView, 20.0f, 120.0f);
        ImGui::DragFloat("Near Clip", &camera.NearClip, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Far Clip", &camera.FarClip, 1.0f, 10.0f, 10000.0f);
    }

    void InspectorPanel::DrawLight(Entity entity)
    {
        auto& light = entity.GetComponent<LightComponent>();
        if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        const char* lightTypeNames[] = { "Directional", "Point" };
        int lightType = static_cast<int>(light.Type);
        if (ImGui::Combo("Type", &lightType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames)))
        {
            light.Type = static_cast<LightComponent::LightType>(lightType);
        }

        ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
        ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f);
    }
}
