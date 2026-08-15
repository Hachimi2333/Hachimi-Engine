#include "Panels/InspectorPanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace HachimiEngine
{
    namespace
    {
        // Draws a collapsible component header with a right-aligned remove button.
        template<typename T>
        bool DrawComponentHeader(Entity entity, const char* label, bool defaultOpen, bool& removed)
        {
            const ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
            const bool open = ImGui::CollapsingHeader(label, flags);

            const float buttonWidth = ImGui::GetFrameHeight();
            ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - buttonWidth, 0.0f));

            ImGui::PushID(label);
            const bool removeClicked = ImGui::SmallButton("x");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove component");
            }
            ImGui::PopID();

            if (removeClicked)
            {
                entity.RemoveComponent<T>();
                removed = true;
            }

            return open;
        }
    }

    void InspectorPanel::Draw(EditorContext& context)
    {
        ImGui::Begin("Inspector");

        if (!context.SelectedEntity || context.ActiveScene == nullptr)
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
            context.ActiveScene->DestroyEntity(entity);
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
            if (!entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Transform Component"))
            {
                entity.AddComponent<TransformComponent>();
            }
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

        bool removed = false;
        const bool open = DrawComponentHeader<TransformComponent>(entity, "Transform", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& transform = entity.Transform();
        ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.05f);
        ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.25f);
        ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.05f, 0.01f, 100.0f);
    }

    void InspectorPanel::DrawMesh(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<MeshComponent>(entity, "Mesh", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& mesh = entity.GetComponent<MeshComponent>();

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
        bool removed = false;
        const bool open = DrawComponentHeader<CameraComponent>(entity, "Camera", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& camera = entity.GetComponent<CameraComponent>();

        ImGui::Checkbox("Primary", &camera.Primary);
        ImGui::SliderFloat("Field Of View", &camera.FieldOfView, 20.0f, 120.0f);
        ImGui::DragFloat("Near Clip", &camera.NearClip, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Far Clip", &camera.FarClip, 1.0f, 10.0f, 10000.0f);
    }

    void InspectorPanel::DrawLight(Entity entity)
    {
        bool removed = false;
        const bool open = DrawComponentHeader<LightComponent>(entity, "Light", true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& light = entity.GetComponent<LightComponent>();

        const char* lightTypeNames[] = { "Directional", "Point" };
        int lightType = static_cast<int>(light.Type);
        if (ImGui::Combo("Type", &lightType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames)))
        {
            light.Type = static_cast<LightComponent::LightType>(lightType);
        }

        ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
        ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f);
        if (light.Type == LightComponent::LightType::Point)
        {
            ImGui::DragFloat("Range", &light.Range, 0.1f, 0.1f, 1000.0f);
        }
        else
        {
            ImGui::Checkbox("Casts Shadows", &light.CastsShadows);
            ImGui::DragFloat("Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.5f");
        }
    }
}
