#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Editor/SceneDirtyState.h"
#include "Panels/EditorContext.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Math/Math.h"

#include <ImGuizmo.h>

namespace HachimiEngine
{
    void DrawTransformComponent(Entity entity, InspectorDrawContext& context)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<TransformComponent>::value());
        if (descriptor == nullptr || !entity.HasComponent<TransformComponent>())
        {
            return;
        }

        bool removed = false;
        const bool open = DrawComponentHeader(entity, *descriptor, true, removed);
        if (removed || !open)
        {
            return;
        }

        auto& transform = entity.Transform();

        // While the gizmo is being dragged it owns the transform; recording the inspector's values
        // at the same time would stack two edits of one value on top of each other.
        const bool gizmoActive = ImGuizmo::IsUsingAny();

        const auto applyEdit = [&](const Math::Vec3& position, const Math::Vec3& rotation, const Math::Vec3& scale)
        {
            if (gizmoActive || context.Context.History == nullptr)
            {
                if (!gizmoActive && context.Context.DirtyState != nullptr)
                {
                    context.Context.DirtyState->MarkDirty();
                }
                return;
            }

            if (Scope<EditorCommand> command = SceneCommands::MakeSetTransform(entity, position, rotation, scale))
            {
                command->Apply(*entity.GetScene());
                RecordEdit(context.Context.History, std::move(command));
            }
        };

        if (BeginInspectorTable("InspectorTransformRows"))
        {
            BeginInspectorProperty("Position");
            {
                Math::Vec3 value = transform.Position;
                if (ImGui::DragFloat3("##Position", Math::ValuePtr(value), 0.05f, 0.0f, 0.0f, "%.3f", ImGuiSliderFlags_ColorMarkers))
                {
                    transform.Position = value;
                    applyEdit(transform.Position, transform.Rotation, transform.Scale);
                }
            }

            BeginInspectorProperty("Rotation");
            {
                Math::Vec3 value = transform.Rotation;
                if (ImGui::DragFloat3("##Rotation", Math::ValuePtr(value), 0.25f, 0.0f, 0.0f, "%.3f", ImGuiSliderFlags_ColorMarkers))
                {
                    transform.Rotation = value;
                    applyEdit(transform.Position, transform.Rotation, transform.Scale);
                }
            }

            BeginInspectorProperty("Scale");
            {
                Math::Vec3 value = transform.Scale;
                if (ImGui::DragFloat3("##Scale", Math::ValuePtr(value), 0.05f, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_ColorMarkers))
                {
                    transform.Scale = value;
                    applyEdit(transform.Position, transform.Rotation, transform.Scale);
                }
            }

            ImGui::EndTable();
        }
    }
}
