#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/UUID.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // Scene owns an EnTT registry and helpers for entity hierarchy and rendering.
    class Scene
    {
    public:
        Scene();
        ~Scene() = default;

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);
        Entity DuplicateEntity(Entity entity);

        Entity GetEntityByUUID(UUID uuid);
        Entity GetPrimaryCameraEntity();

        std::vector<Entity> GetAllEntities();

        glm::mat4 GetWorldTransform(entt::entity entity) const;

        void SetViewportSize(uint32_t width, uint32_t height);
        void OnUpdate(Timestep timestep);
        void OnRender(const EditorCamera& camera);

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        entt::registry& GetRegistry() { return m_Registry; }
        const std::unordered_map<UUID, entt::entity>& GetEntityMap() const { return m_EntityMap; }

    private:
        void DestroyChildren(entt::entity entity);
        void ApplyLightsToRenderer();

    private:
        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityMap;

        std::string m_Name = "Untitled Scene";
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;

        friend class Entity;
        friend class SceneSerializer;
    };
}
