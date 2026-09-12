#include "Components/ComponentDrawers.h"

#include "Components/InspectorWidgets.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/TransformComponent.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    void DrawTransformComponent(Entity entity, InspectorDrawContext& context)
    {
        (void)context;

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
        if (BeginInspectorTable("InspectorTransformRows"))
        {
            BeginInspectorProperty("Position");
            ImGui::DragFloat3("##Position", Math::ValuePtr(transform.Position), 0.05f, 0.0f, 0.0f, "%.3f", ImGuiSliderFlags_ColorMarkers);

            BeginInspectorProperty("Rotation");
            ImGui::DragFloat3("##Rotation", Math::ValuePtr(transform.Rotation), 0.25f, 0.0f, 0.0f, "%.3f", ImGuiSliderFlags_ColorMarkers);

            BeginInspectorProperty("Scale");
            ImGui::DragFloat3("##Scale", Math::ValuePtr(transform.Scale), 0.05f, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_ColorMarkers);

            ImGui::EndTable();
        }
    }
}
