#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    void DrawRigidbodyComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<RigidbodyComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<RigidbodyComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& rigidbody = entity.GetComponent<RigidbodyComponent>();

        if (BeginInspectorTable("InspectorRigidbodyRows"))
        {
            const char* typeNames[] = { "Static", "Kinematic", "Dynamic" };
            int type = static_cast<int>(rigidbody.Type);

            BeginInspectorProperty("Type");
            if (ImGui::Combo("##Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
            {
                rigidbody.Type = static_cast<RigidbodyComponent::RigidbodyType>(type);
            }

            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Static)
            {
                BeginInspectorProperty("Linear Velocity");
                ImGui::DragFloat3("##Linear Velocity", Math::ValuePtr(rigidbody.LinearVelocity), 0.05f);

                BeginInspectorProperty("Angular Velocity");
                ImGui::DragFloat3("##Angular Velocity", Math::ValuePtr(rigidbody.AngularVelocity), 0.05f);
            }

            BeginInspectorProperty("Linear Damping");
            ImGui::DragFloat("##Linear Damping", &rigidbody.LinearDamping, 0.01f, 0.0f, 10.0f);

            BeginInspectorProperty("Angular Damping");
            ImGui::DragFloat("##Angular Damping", &rigidbody.AngularDamping, 0.01f, 0.0f, 10.0f);

            BeginInspectorProperty("Gravity Scale");
            ImGui::DragFloat("##Gravity Scale", &rigidbody.GravityScale, 0.05f, 0.0f, 10.0f);

            BeginInspectorProperty("Enable Sleep");
            ImGui::Checkbox("##Enable Sleep", &rigidbody.EnableSleep);

            BeginInspectorProperty("Initially Awake");
            ImGui::Checkbox("##Initially Awake", &rigidbody.InitiallyAwake);

            BeginInspectorProperty("Is Bullet");
            ImGui::Checkbox("##Is Bullet", &rigidbody.IsBullet);

            BeginInspectorProperty("Enabled");
            ImGui::Checkbox("##Enabled", &rigidbody.IsEnabled);

            ImGui::EndTable();
        }
    }

    void DrawColliderComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<ColliderComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<ColliderComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& collider = entity.GetComponent<ColliderComponent>();

        if (BeginInspectorTable("InspectorColliderRows"))
        {
            const char* shapeNames[] = { "Box", "Sphere", "Capsule", "Plane" };
            int shapeType = static_cast<int>(collider.ShapeType);

            BeginInspectorProperty("Shape");
            if (ImGui::Combo("##Shape", &shapeType, shapeNames, IM_ARRAYSIZE(shapeNames)))
            {
                collider.ShapeType = static_cast<ColliderComponent::ColliderShapeType>(shapeType);
            }

            switch (collider.ShapeType)
            {
                case ColliderComponent::ColliderShapeType::Box:
                    BeginInspectorProperty("Half Extents");
                    ImGui::DragFloat3("##Half Extents", Math::ValuePtr(collider.HalfExtents), 0.05f, 0.01f, 100.0f);
                    break;
                case ColliderComponent::ColliderShapeType::Sphere:
                    BeginInspectorProperty("Radius");
                    ImGui::DragFloat("##Radius", &collider.Radius, 0.05f, 0.01f, 100.0f);
                    break;
                case ColliderComponent::ColliderShapeType::Capsule:
                    BeginInspectorProperty("Radius");
                    ImGui::DragFloat("##Radius", &collider.Radius, 0.05f, 0.01f, 100.0f);

                    BeginInspectorProperty("Height");
                    ImGui::DragFloat("##Height", &collider.Height, 0.05f, 0.01f, 100.0f);
                    break;
                case ColliderComponent::ColliderShapeType::Plane:
                    BeginInspectorProperty("Half Width");
                    ImGui::DragFloat("##Half Width", &collider.HalfExtents.x, 0.05f, 0.01f, 1000.0f);

                    BeginInspectorProperty("Half Depth");
                    ImGui::DragFloat("##Half Depth", &collider.HalfExtents.z, 0.05f, 0.01f, 1000.0f);
                    break;
            }

            BeginInspectorProperty("Offset");
            ImGui::DragFloat3("##Offset", Math::ValuePtr(collider.Offset), 0.05f);

            BeginInspectorProperty("Density");
            ImGui::DragFloat("##Density", &collider.Density, 0.05f, 0.0f, 100000.0f);

            BeginInspectorProperty("Friction");
            ImGui::SliderFloat("##Friction", &collider.Friction, 0.0f, 1.0f);

            BeginInspectorProperty("Restitution");
            ImGui::SliderFloat("##Restitution", &collider.Restitution, 0.0f, 1.0f);

            BeginInspectorProperty("Rolling Resistance");
            ImGui::SliderFloat("##Rolling Resistance", &collider.RollingResistance, 0.0f, 1.0f);

            BeginInspectorProperty("Is Trigger");
            ImGui::Checkbox("##Is Trigger", &collider.IsTrigger);

            BeginInspectorProperty("Category Bits");
            ImGui::InputScalar("##Category Bits", ImGuiDataType_U64, &collider.CategoryBits, nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal);

            BeginInspectorProperty("Mask Bits");
            ImGui::InputScalar("##Mask Bits", ImGuiDataType_U64, &collider.MaskBits, nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal);

            ImGui::EndTable();
        }
    }
}
