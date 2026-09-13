#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Editor/SceneDirtyState.h"
#include "Panels/EditorContext.h"
#include "Renderer/MeshFactory.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Entity.h"
#include "Math/Math.h"

#include <array>

namespace HachimiEngine
{
    namespace
    {
        // Combo labels in the order of PrimitiveMeshType, so index arithmetic stays trivial.
        constexpr std::array<const char*, 4> PrimitiveNames { "Cube", "Sphere", "Plane", "Grid" };
    }

    void DrawMeshRendererComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<MeshRendererComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<MeshRendererComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            return;
        }

        auto& mesh = entity.GetComponent<MeshRendererComponent>();
        CommandHistory* history = context.Context.History;

        if (BeginInspectorTable("InspectorMeshRows"))
        {
            // PrimitiveMeshType::None has no entry in the combo, so clamp into range.
            int primitiveType = static_cast<int>(mesh.Primitive) - static_cast<int>(PrimitiveMeshType::Cube);
            primitiveType = primitiveType < 0 ? 0 : primitiveType;

            BeginInspectorProperty("Primitive");
            if (ImGui::Combo("##Primitive", &primitiveType, PrimitiveNames.data(), static_cast<int>(PrimitiveNames.size())))
            {
                const int requested = primitiveType + static_cast<int>(PrimitiveMeshType::Cube);
                // The command owns both the enum and the mesh it replaces, so undo restores the
                // geometry as well as the dropdown.
                if (Scope<EditorCommand> command = SceneCommands::MakeSetPrimitive(entity, requested))
                {
                    command->Apply(*entity.GetScene());
                    RecordEdit(history, std::move(command));
                }
            }

            BeginInspectorProperty("Material");
            DrawAssetField("##Material", entity, AssetFieldMaterialSlot, mesh.Material, AssetType::Material,
                context.AssetFields, context);

            // The inline values are what the entity shades with when no material asset is assigned,
            // and the fallback for the channels a material does not set, so they stay editable.
            BeginInspectorProperty("Albedo Color");
            {
                Math::Vec4 color = mesh.AlbedoColor;
                if (ImGui::ColorEdit4("##Albedo Color", Math::ValuePtr(color)))
                {
                    mesh.AlbedoColor = color;
                    RecordEdit(history, SceneCommands::MakeSetMeshColor(entity, color));
                }
            }

            BeginInspectorProperty("Roughness");
            {
                float roughness = mesh.Roughness;
                if (ImGui::SliderFloat("##Roughness", &roughness, 0.01f, 1.0f))
                {
                    mesh.Roughness = roughness;
                    RecordEdit(history, SceneCommands::MakeSetMeshRoughness(entity, roughness));
                }
            }

            BeginInspectorProperty("Metallic");
            {
                float metallic = mesh.Metallic;
                if (ImGui::SliderFloat("##Metallic", &metallic, 0.0f, 1.0f))
                {
                    mesh.Metallic = metallic;
                    RecordEdit(history, SceneCommands::MakeSetMeshMetallic(entity, metallic));
                }
            }

            BeginInspectorProperty("Visible");
            ImGui::Checkbox("##Visible", &mesh.Visible);

            ImGui::EndTable();
        }
    }

    void DrawCameraComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<CameraComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<CameraComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            return;
        }

        auto& camera = entity.GetComponent<CameraComponent>();
        CommandHistory* history = context.Context.History;

        if (BeginInspectorTable("InspectorCameraRows"))
        {
            BeginInspectorProperty("Primary");
            ImGui::Checkbox("##Primary", &camera.Primary);

            BeginInspectorProperty("Field Of View");
            {
                float fieldOfView = camera.FieldOfView;
                if (ImGui::SliderFloat("##Field Of View", &fieldOfView, 20.0f, 120.0f))
                {
                    camera.FieldOfView = fieldOfView;
                    RecordEdit(history, SceneCommands::MakeSetCameraFieldOfView(entity, fieldOfView));
                }
            }

            BeginInspectorProperty("Near Clip");
            {
                float nearClip = camera.NearClip;
                if (ImGui::DragFloat("##Near Clip", &nearClip, 0.01f, 0.001f, 10.0f))
                {
                    camera.NearClip = nearClip;
                    RecordEdit(history, SceneCommands::MakeSetCameraNearClip(entity, nearClip));
                }
            }

            BeginInspectorProperty("Far Clip");
            {
                float farClip = camera.FarClip;
                if (ImGui::DragFloat("##Far Clip", &farClip, 1.0f, 10.0f, 10000.0f))
                {
                    camera.FarClip = farClip;
                    RecordEdit(history, SceneCommands::MakeSetCameraFarClip(entity, farClip));
                }
            }

            ImGui::EndTable();
        }
    }

    void DrawLightComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<LightComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<LightComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            return;
        }

        auto& light = entity.GetComponent<LightComponent>();
        CommandHistory* history = context.Context.History;

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
            {
                Math::Vec3 color = light.Color;
                if (ImGui::ColorEdit3("##Color", Math::ValuePtr(color)))
                {
                    light.Color = color;
                    RecordEdit(history, SceneCommands::MakeSetLightColor(entity, color));
                }
            }

            BeginInspectorProperty("Intensity");
            {
                float intensity = light.Intensity;
                if (ImGui::DragFloat("##Intensity", &intensity, 0.1f, 0.0f, 1000.0f))
                {
                    light.Intensity = intensity;
                    RecordEdit(history, SceneCommands::MakeSetLightIntensity(entity, intensity));
                }
            }

            if (light.Type == LightComponent::LightType::Point)
            {
                BeginInspectorProperty("Range");
                {
                    float range = light.Range;
                    if (ImGui::DragFloat("##Range", &range, 0.1f, 0.1f, 1000.0f))
                    {
                        light.Range = range;
                        RecordEdit(history, SceneCommands::MakeSetLightRange(entity, range));
                    }
                }
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
