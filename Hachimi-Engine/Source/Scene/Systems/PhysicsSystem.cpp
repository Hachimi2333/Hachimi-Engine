#include "Scene/Systems/PhysicsSystem.h"

#include "Core/Log.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Scene.h"

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
