#include "Scene/Entity.h"

#include "Scene/Scene.h"

namespace HachimiEngine
{
    Entity::Entity(entt::entity entityHandle, Scene* scene)
        : m_Handle(scene->m_Registry, entityHandle)
    {
    }
}
