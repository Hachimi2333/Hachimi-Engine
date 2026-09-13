#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Editor/SceneDirtyState.h"
#include "Panels/EditorContext.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Entity.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    void DrawRigidbodyComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<RigidbodyComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<RigidbodyComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            return;
        }

        auto& rigidbody = entity.GetComponent<RigidbodyComponent>();
        CommandHistory* history = context.Context.History;

        if (BeginInspectorTable("InspectorRigidbodyRows"))
        {
            const char* typeNames[] = { "Static", "Kinematic", "Dynamic" };
            int type = static_cast<int>(rigidbody.Type);

            BeginInspectorProperty("Type");
            if (ImGui::Combo("##Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
            {
                rigidbody.Type = static_cast<RigidbodyComponent::RigidbodyType>(type);
                if (context.Context.DirtyState != nullptr)
                {
                    context.Context.DirtyState->MarkDirty();
                }
            }

            if (rigidbody.Type != RigidbodyComponent::RigidbodyType::Static)
            {
                BeginInspectorProperty("Linear Velocity");
                ImGui::DragFloat3("##Linear Velocity", Math::ValuePtr(rigidbody.LinearVelocity), 0.05f);

                BeginInspectorProperty("Angular Velocity");
                ImGui::DragFloat3("##Angular Velocity", Math::ValuePtr(rigidbody.AngularVelocity), 0.05f);
            }

            BeginInspectorProperty("Linear Damping");
            {
                float damping = rigidbody.LinearDamping;
                if (ImGui::DragFloat("##Linear Damping", &damping, 0.01f, 0.0f, 10.0f))
                {
                    rigidbody.LinearDamping = damping;
                    RecordEdit(history, SceneCommands::MakeSetRigidbodyLinearDamping(entity, damping));
                }
            }

            BeginInspectorProperty("Angular Damping");
            {
                float damping = rigidbody.AngularDamping;
                if (ImGui::DragFloat("##Angular Damping", &damping, 0.01f, 0.0f, 10.0f))
                {
                    rigidbody.AngularDamping = damping;
                    RecordEdit(history, SceneCommands::MakeSetRigidbodyAngularDamping(entity, damping));
                }
            }

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
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<ColliderComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<ColliderComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeaderUndoable(entity, *descriptor, true, removed, context.Context.History);
        if (removed || !open)
        {
            return;
        }

        auto& collider = entity.GetComponent<ColliderComponent>();
        CommandHistory* history = context.Context.History;

        if (BeginInspectorTable("InspectorColliderRows"))
        {
            const char* shapeNames[] = { "Box", "Sphere", "Capsule", "Plane" };
            int shapeType = static_cast<int>(collider.ShapeType);

            BeginInspectorProperty("Shape");
            if (ImGui::Combo("##Shape", &shapeType, shapeNames, IM_ARRAYSIZE(shapeNames)))
            {
                collider.ShapeType = static_cast<ColliderComponent::ColliderShapeType>(shapeType);
                if (context.Context.DirtyState != nullptr)
                {
                    context.Context.DirtyState->MarkDirty();
                }
            }

            switch (collider.ShapeType)
            {
                case ColliderComponent::ColliderShapeType::Box:
                    BeginInspectorProperty("Half Extents");
                    ImGui::DragFloat3("##Half Extents", Math::ValuePtr(collider.HalfExtents), 0.05f, 0.01f, 100.0f);
                    break;
                case ColliderComponent::ColliderShapeType::Sphere:
                    BeginInspectorProperty("Radius");
                    {
                        float radius = collider.Radius;
                        if (ImGui::DragFloat("##Radius", &radius, 0.05f, 0.01f, 100.0f))
                        {
                            collider.Radius = radius;
                            RecordEdit(history, SceneCommands::MakeSetColliderRadius(entity, radius));
                        }
                    }
                    break;
                case ColliderComponent::ColliderShapeType::Capsule:
                    BeginInspectorProperty("Radius");
                    {
                        float radius = collider.Radius;
                        if (ImGui::DragFloat("##Radius", &radius, 0.05f, 0.01f, 100.0f))
                        {
                            collider.Radius = radius;
                            RecordEdit(history, SceneCommands::MakeSetColliderRadius(entity, radius));
                        }
                    }

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
            {
                float friction = collider.Friction;
                if (ImGui::SliderFloat("##Friction", &friction, 0.0f, 1.0f))
                {
                    collider.Friction = friction;
                    RecordEdit(history, SceneCommands::MakeSetColliderFriction(entity, friction));
                }
            }

            BeginInspectorProperty("Restitution");
            {
                float restitution = collider.Restitution;
                if (ImGui::SliderFloat("##Restitution", &restitution, 0.0f, 1.0f))
                {
                    collider.Restitution = restitution;
                    RecordEdit(history, SceneCommands::MakeSetColliderRestitution(entity, restitution));
                }
            }

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
