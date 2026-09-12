#include "Scene/Entity.h"

#include "Scene/Scene.h"

namespace HachimiEngine
{
    Entity::Entity(entt::entity entityHandle, Scene* scene)
        : m_Handle(scene->m_Registry, entityHandle)
    {
    }

    entt::registry& Entity::GetRegistry() const
    {
        entt::registry* registry = m_Handle.registry();
        HE_CORE_ASSERT(registry != nullptr);
        return *registry;
    }
}
