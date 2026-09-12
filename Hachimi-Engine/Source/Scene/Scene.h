#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Physics/PhysicsWorld.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/RenderView.h"
#include "Scene/Entity.h"
#include "Scene/SceneSystem.h"
#include "Scripting/ScriptWorld.h"
#include "Math/Math.h"

#include <entt/entt.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // Camera and options for one extracted frame. The scene turns this into a RenderView;
    // the caller decides which SceneRenderer and which render target consume it.
    struct SceneRenderDesc
    {
        Math::Mat4 View { 1.0f };
        Math::Mat4 Projection { 1.0f };
        Math::Vec3 CameraPosition { 0.0f };
        bool DrawGrid = false;
    };

    // Scene owns an EnTT registry and helpers for entity hierarchy and rendering.
    //
    // Components are handled generically through ComponentRegistry, so this class never has to
    // list them: a component is created with an entity, duplicated, cloned and serialized
    // because its descriptor says how, not because someone remembered to add a branch here.
    class Scene
    {
    public:
        Scene();
        ~Scene() = default;

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);
        Entity DuplicateEntity(Entity entity);

        // Creates a runtime copy with matching UUIDs; mesh assets are shared while material overrides are cloned.
        Ref<Scene> Clone() const;

        Entity GetEntityByUUID(UUID uuid);
        Entity GetPrimaryCameraEntity();

        std::vector<Entity> GetAllEntities();

        // Hierarchy. The parent link is the single source of truth; the child lookup is derived
        // from it, so the two directions cannot disagree.
        void SetParent(Entity child, Entity parent);
        void ClearParent(Entity child);
        std::vector<Entity> GetChildren(Entity parent);
        bool IsAncestorOf(Entity ancestor, Entity candidate);

        // World-space transform of an entity, composed through its parent chain.
        Math::Mat4 GetWorldTransform(entt::entity entity) const;

        void SetViewportSize(uint32_t width, uint32_t height);

        // Attaches the systems a running scene needs - physics and scripting - and detaches them
        // again on stop. Both are independent, so a scene without a physics world still runs its
        // scripts.
        void OnRuntimeStart();
        void OnRuntimeStop();
        bool IsRuntimeRunning() const { return m_RuntimeRunning; }
        // Advances every attached system, phase by phase.
        void OnUpdate(Timestep timestep);

        // Simulation steps. Presentation order is phase order; within a phase, insertion order.
        // Systems are attached and detached outside OnUpdate.
        template<typename T, typename... Args>
        T& AddSystem(Args&&... args);
        void RemoveSystem(SceneSystem& system);
        // Returns nullptr when no system of that type is attached.
        template<typename T>
        T* FindSystem();
        template<typename T>
        const T* FindSystem() const;

        // Convenience accessors for the two systems the runtime attaches.
        bool IsPhysicsRunning() const;
        bool IsScriptRunning() const;

        // Extracts everything a renderer needs for one frame, as plain data. Rendering itself is
        // the caller's job, which is why the editor viewport, the game panel and the Player can
        // share this without sharing renderer state.
        RenderView BuildRenderView(const SceneRenderDesc& desc) const;
        RenderView BuildRenderView(const EditorCamera& camera, bool drawGrid) const;

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        EnvironmentSettings& GetEnvironmentSettings() { return m_Environment; }
        const EnvironmentSettings& GetEnvironmentSettings() const { return m_Environment; }

        PhysicsSettings& GetPhysicsSettings() { return m_PhysicsSettings; }
        const PhysicsSettings& GetPhysicsSettings() const { return m_PhysicsSettings; }

        entt::registry& GetRegistry() { return m_Registry; }
        const std::unordered_map<UUID, entt::entity>& GetEntityMap() const { return m_EntityMap; }

    private:
        void DestroyChildren(entt::entity entity);
        void CollectLights(LightingEnvironment& outLighting) const;

        // Composes the parent chain, guarding against a malformed or over-long one.
        Math::Mat4 GetWorldTransformRecursive(entt::entity entity, size_t depth) const;

        // True when making parentUUID the parent of child would close a loop.
        bool WouldCreateCycle(entt::entity child, UUID parentUUID) const;

        void RebuildChildrenIndexIfDirty();
        UUID GetEntityUUID(entt::entity entity) const;

        // Inserts by phase, keeping insertion order inside a phase.
        SceneSystem& AddSystemImpl(Scope<SceneSystem> system);

    private:
        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityMap;

        // Derived from RelationshipComponent::Parent. Rebuilt on demand after any hierarchy
        // change rather than mirrored on every edit.
        std::unordered_map<UUID, std::vector<UUID>> m_ChildrenIndex;
        bool m_ChildrenIndexDirty = false;

        std::string m_Name = "Untitled Scene";
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;
        EnvironmentSettings m_Environment;
        PhysicsSettings m_PhysicsSettings;

        std::vector<Scope<SceneSystem>> m_Systems;
        // The systems OnRuntimeStart attached, so stopping detaches exactly those.
        std::vector<SceneSystem*> m_RuntimeSystems;
        bool m_RuntimeRunning = false;

        friend class Entity;
        friend class SceneSerializer;
    };

    template<typename T, typename... Args>
    T& Scene::AddSystem(Args&&... args)
    {
        Scope<T> system = CreateScope<T>(std::forward<Args>(args)...);
        T& reference = *system;
        AddSystemImpl(std::move(system));
        return reference;
    }

    template<typename T>
    T* Scene::FindSystem()
    {
        for (const Scope<SceneSystem>& system : m_Systems)
        {
            // dynamic_cast rather than a name comparison: a system that lies about its name
            // would otherwise be reinterpreted as the wrong type.
            if (T* typed = dynamic_cast<T*>(system.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }

    template<typename T>
    const T* Scene::FindSystem() const
    {
        for (const Scope<SceneSystem>& system : m_Systems)
        {
            if (const T* typed = dynamic_cast<const T*>(system.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }
}
