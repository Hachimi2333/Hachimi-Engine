#pragma once

#include "Components/InspectorRegistry.h"

namespace HachimiEngine
{
    // Component drawers, one per registered component type. Each is a plain function: the state
    // a drawer needs travels in InspectorDrawContext, so a drawer has no lifetime of its own.
    void DrawTransformComponent(Entity entity, InspectorDrawContext& context);
    void DrawRigidbodyComponent(Entity entity, InspectorDrawContext& context);
    void DrawColliderComponent(Entity entity, InspectorDrawContext& context);
    void DrawMeshComponent(Entity entity, InspectorDrawContext& context);
    void DrawCameraComponent(Entity entity, InspectorDrawContext& context);
    void DrawLightComponent(Entity entity, InspectorDrawContext& context);
    void DrawScriptComponent(Entity entity, InspectorDrawContext& context);
}
