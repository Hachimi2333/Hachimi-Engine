#include "Scene/Systems/PhysicsSystem.h"

#include "Core/Log.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Events/PhysicsSceneEvents.h"
#include "Scene/Scene.h"

#include <entt/entt.hpp>

namespace HachimiEngine
{
    void PhysicsSystem::OnAttach(Scene& scene)
    {
        m_World = CreateScope<PhysicsWorld>(scene.GetPhysicsSettings());
        if (!m_World->IsRunning())
        {
            // Keep the system attached but inert, so a Box3D failure is reported once here
            // instead of disabling whatever else the scene runs.
            HE_CORE_ERROR("Physics world could not be created; the scene will simulate without physics");
            m_World = nullptr;
            return;
        }

        m_World->CreateBodies(scene);
    }

    void PhysicsSystem::OnUpdate(Scene& scene, Timestep timestep)
    {
        if (m_World == nullptr)
        {
            return;
        }

        m_World->Update(scene, timestep);

        // Published right after the step and before the update phase, so a handler subscribed from a
        // system or from a script sees the step that just ran and nothing later. One b3World_Step is
        // one batch: a frame that owed two steps publishes two, in the order they were simulated,
        // rather than merging them into one event a handler cannot order.
        PublishEvents(scene);
    }

    void PhysicsSystem::PublishEvents(Scene& scene) const
    {
        const PhysicsEventQueue& events = m_World->GetEvents();
        if (events.IsEmpty())
        {
            return;
        }

        entt::dispatcher& dispatcher = scene.GetDispatcher();

        if (!events.GetCollisionBegin().empty() || !events.GetCollisionEnd().empty()
            || !events.GetCollisionHit().empty())
        {
            SceneCollisionEvent collision;
            collision.Begins = events.GetCollisionBegin();
            collision.Ends = events.GetCollisionEnd();
            collision.Hits = events.GetCollisionHit();
            dispatcher.trigger(collision);
        }

        if (!events.GetTriggerBegin().empty() || !events.GetTriggerEnd().empty())
        {
            SceneTriggerEvent trigger;
            trigger.Begins = events.GetTriggerBegin();
            trigger.Ends = events.GetTriggerEnd();
            dispatcher.trigger(trigger);
        }
    }

    void PhysicsSystem::OnDetach(Scene& scene)
    {
        (void)scene;
        m_World = nullptr;
    }

    bool PhysicsSystem::IsRunning() const
    {
        return m_World != nullptr && m_World->IsRunning();
    }
}
