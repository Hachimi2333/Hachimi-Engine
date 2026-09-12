#include "Components/InspectorRegistry.h"

#include "Components/ComponentDrawers.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Components/TransformComponent.h"

#include <unordered_map>

namespace HachimiEngine
{
    namespace
    {
        std::unordered_map<entt::id_type, ComponentDrawFn>& Drawers()
        {
            static std::unordered_map<entt::id_type, ComponentDrawFn> drawers;
            return drawers;
        }

        bool& BuiltinsRegistered()
        {
            static bool registered = false;
            return registered;
        }
    }

    void InspectorRegistry::Register(entt::id_type typeId, ComponentDrawFn draw)
    {
        HE_CORE_ASSERT(draw != nullptr);
        HE_CORE_ASSERT(!Drawers().contains(typeId));
        Drawers()[typeId] = draw;
    }

    ComponentDrawFn InspectorRegistry::Find(entt::id_type typeId)
    {
        EnsureBuiltinDrawersRegistered();

        const auto it = Drawers().find(typeId);
        return it != Drawers().end() ? it->second : nullptr;
    }

    void InspectorRegistry::EnsureBuiltinDrawersRegistered()
    {
        if (BuiltinsRegistered())
        {
            return;
        }

        BuiltinsRegistered() = true;
        RegisterBuiltinDrawers();
    }

    void InspectorRegistry::RegisterBuiltinDrawers()
    {
        // Ordered like the engine registry; the panel iterates that one, so this table only has
        // to answer "who draws this type".
        Register(entt::type_hash<TransformComponent>::value(), &DrawTransformComponent);
        Register(entt::type_hash<RigidbodyComponent>::value(), &DrawRigidbodyComponent);
        Register(entt::type_hash<ColliderComponent>::value(), &DrawColliderComponent);
        Register(entt::type_hash<MeshComponent>::value(), &DrawMeshComponent);
        Register(entt::type_hash<CameraComponent>::value(), &DrawCameraComponent);
        Register(entt::type_hash<LightComponent>::value(), &DrawLightComponent);
        Register(entt::type_hash<ScriptComponent>::value(), &DrawScriptComponent);
    }
}
