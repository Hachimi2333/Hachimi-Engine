#include "Scripting/Lua/LuaPhysicsBindings.h"

#include "Core/Log.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Scripting/ScriptEntity.h"

namespace HachimiEngine
{
    void RegisterLuaPhysicsBindings(sol::state& state, sol::table& he, Scene& scene)
    {
        sol::usertype<ScriptEntity> entity = state["ScriptEntity"];
        entity.set_function("GetLinearVelocity", &ScriptEntity::GetLinearVelocity);
        entity.set_function("SetLinearVelocity", &ScriptEntity::SetLinearVelocity);
        entity.set_function("GetAngularVelocity", &ScriptEntity::GetAngularVelocity);
        entity.set_function("SetAngularVelocity", &ScriptEntity::SetAngularVelocity);
        entity.set_function("ApplyForce", &ScriptEntity::ApplyForce);
        entity.set_function("ApplyImpulse", &ScriptEntity::ApplyImpulse);
        entity.set_function("ApplyTorque", &ScriptEntity::ApplyTorque);
        entity.set_function("ApplyAngularImpulse", &ScriptEntity::ApplyAngularImpulse);
        entity.set_function("Teleport", &ScriptEntity::Teleport);
        entity.set_function("IsTriggered", &ScriptEntity::IsTriggered);
        entity.set_function("SetTriggered", &ScriptEntity::SetTriggered);

        // One warning per VM, shared by all query functions.
        auto warned = CreateRef<bool>(false);
        auto getWorld = [&scene, warned]() -> PhysicsWorld*
        {
            auto* system = scene.FindSystem<PhysicsSystem>();
            if (system != nullptr && system->GetWorld() != nullptr) return system->GetWorld();
            if (!*warned)
            {
                HE_CORE_WARN("HE.Physics query requested without a physics world");
                *warned = true;
            }
            return nullptr;
        };

        sol::table physics = state.create_table();
        he["Physics"] = physics;
        physics.set_function("Raycast", [&state, &scene, getWorld](const Math::Vec3& origin,
            const Math::Vec3& direction, float maxDistance) -> sol::object
        {
            auto* world = getWorld();
            RayCastResult hit;
            if (world == nullptr || !world->RayCast(origin, direction, maxDistance, hit)) return sol::nil;
            Entity owner = scene.GetEntityByKey(hit.EntityKey);
            if (!owner) return sol::nil;
            sol::table result = state.create_table();
            result["entity"] = ScriptEntity(owner, &scene);
            result["point"] = hit.Point;
            result["normal"] = hit.Normal;
            result["distance"] = hit.Distance;
            return result;
        });
        physics.set_function("OverlapSphere", [&state, &scene, getWorld](const Math::Vec3& center, float radius)
        {
            sol::table result = state.create_table();
            if (auto* world = getWorld())
            {
                OverlapResults hits;
                world->OverlapSphere(center, radius, hits);
                int index = 1;
                for (const auto& hit : hits)
                    if (Entity owner = scene.GetEntityByKey(hit.EntityKey))
                        result[index++] = ScriptEntity(owner, &scene);
            }
            return result;
        });
    }
}
