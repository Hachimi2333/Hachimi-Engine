#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Renderer/MeshFactory.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshComponent.h"
#include "Math/Math.h"

#include <array>

namespace HachimiEngine
{
    namespace
    {
        // Combo labels in the order of PrimitiveMeshType, so index arithmetic stays trivial.
        constexpr std::array<const char*, 4> PrimitiveNames { "Cube", "Sphere", "Plane", "Grid" };
    }

    void DrawMeshComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<MeshComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<MeshComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& mesh = entity.GetComponent<MeshComponent>();

        if (BeginInspectorTable("InspectorMeshRows"))
        {
            // PrimitiveMeshType::None has no entry in the combo, so clamp into range.
            int primitiveType = static_cast<int>(mesh.PrimitiveType) - static_cast<int>(PrimitiveMeshType::Cube);
            primitiveType = primitiveType < 0 ? 0 : primitiveType;

            BeginInspectorProperty("Primitive");
            if (ImGui::Combo("##Primitive", &primitiveType, PrimitiveNames.data(), static_cast<int>(PrimitiveNames.size())))
            {
                mesh.PrimitiveType = static_cast<PrimitiveMeshType>(primitiveType + static_cast<int>(PrimitiveMeshType::Cube));
                mesh.Mesh = MeshFactory::CreatePrimitive(mesh.PrimitiveType);
            }

            BeginInspectorProperty("Albedo Color");
            ImGui::ColorEdit4("##Albedo Color", Math::ValuePtr(mesh.MaterialColor));

            BeginInspectorProperty("Roughness");
            ImGui::SliderFloat("##Roughness", &mesh.Roughness, 0.01f, 1.0f);

            BeginInspectorProperty("Metallic");
            ImGui::SliderFloat("##Metallic", &mesh.Metallic, 0.0f, 1.0f);

            BeginInspectorProperty("Visible");
            ImGui::Checkbox("##Visible", &mesh.Visible);

            ImGui::EndTable();
        }
    }

    void DrawCameraComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<CameraComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<CameraComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& camera = entity.GetComponent<CameraComponent>();

        if (BeginInspectorTable("InspectorCameraRows"))
        {
            BeginInspectorProperty("Primary");
            ImGui::Checkbox("##Primary", &camera.Primary);

            BeginInspectorProperty("Field Of View");
            ImGui::SliderFloat("##Field Of View", &camera.FieldOfView, 20.0f, 120.0f);

            BeginInspectorProperty("Near Clip");
            ImGui::DragFloat("##Near Clip", &camera.NearClip, 0.01f, 0.001f, 10.0f);

            BeginInspectorProperty("Far Clip");
            ImGui::DragFloat("##Far Clip", &camera.FarClip, 1.0f, 10.0f, 10000.0f);

            ImGui::EndTable();
        }
    }

    void DrawLightComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<LightComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<LightComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& light = entity.GetComponent<LightComponent>();

        if (BeginInspectorTable("InspectorLightRows"))
        {
            const char* typeNames[] = { "Directional", "Point" };
            int type = static_cast<int>(light.Type);

            BeginInspectorProperty("Type");
            if (ImGui::Combo("##Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
            {
                light.Type = static_cast<LightComponent::LightType>(type);
            }

            BeginInspectorProperty("Color");
            ImGui::ColorEdit3("##Color", Math::ValuePtr(light.Color));

            BeginInspectorProperty("Intensity");
            ImGui::DragFloat("##Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f);

            if (light.Type == LightComponent::LightType::Point)
            {
                BeginInspectorProperty("Range");
                ImGui::DragFloat("##Range", &light.Range, 0.1f, 0.1f, 1000.0f);
            }
            else
            {
                BeginInspectorProperty("Casts Shadows");
                ImGui::Checkbox("##Casts Shadows", &light.CastsShadows);

                BeginInspectorProperty("Shadow Bias");
                ImGui::DragFloat("##Shadow Bias", &light.ShadowBias, 0.0001f, 0.0f, 0.05f, "%.5f");
            }

            ImGui::EndTable();
        }
    }
}
