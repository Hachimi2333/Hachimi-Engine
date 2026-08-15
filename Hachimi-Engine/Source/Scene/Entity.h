#pragma once

#include "Core/Assert.h"
#include "Core/Base.h"
#include "Core/UUID.h"
#include "Scene/Components.h"

#include <entt/entt.hpp>

namespace HachimiEngine
{
    class Scene;

    // Lightweight wrapper around an EnTT handle owned by a Scene registry.
    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity entityHandle, Scene* scene);
        Entity(const Entity&) = default;
        Entity& operator=(const Entity&) = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            HE_CORE_ASSERT(!HasComponent<T>());
            return m_Handle.emplace<T>(std::forward<Args>(args)...);
        }

        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args)
        {
            return m_Handle.emplace_or_replace<T>(std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            HE_CORE_ASSERT(HasComponent<T>());
            return m_Handle.get<T>();
        }

        template<typename T>
        const T& GetComponent() const
        {
            HE_CORE_ASSERT(HasComponent<T>());
            return m_Handle.get<T>();
        }

        template<typename T>
        bool HasComponent() const
        {
            return static_cast<bool>(m_Handle) && m_Handle.all_of<T>();
        }

        template<typename T>
        void RemoveComponent()
        {
            m_Handle.remove<T>();
        }

        UUID GetUUID() const { return GetComponent<IDComponent>().ID; }
        const std::string& GetName() const { return GetComponent<TagComponent>().Tag; }

        TransformComponent& Transform() { return GetComponent<TransformComponent>(); }
        const TransformComponent& Transform() const { return GetComponent<TransformComponent>(); }

        explicit operator bool() const { return static_cast<bool>(m_Handle); }
        operator entt::entity() const { return m_Handle.entity(); }
        operator uint32_t() const { return static_cast<uint32_t>(m_Handle.entity()); }

        entt::entity GetHandle() const { return m_Handle.entity(); }

        bool operator==(const Entity& other) const { return m_Handle == other.m_Handle; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::handle m_Handle;

        friend class Scene;
    };
}
