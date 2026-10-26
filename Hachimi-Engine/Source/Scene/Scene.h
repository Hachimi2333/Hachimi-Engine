#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Physics/PhysicsSettings.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/RenderView.h"
#include "Scene/Entity.h"
#include "Scene/SceneSystem.h"
#include "Scripting/ScriptCommandQueue.h"
#include "Math/Math.h"

#include <entt/entt.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    class InputMap;

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
        ~Scene();

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);
        Entity DuplicateEntity(Entity entity);

        // Creates a runtime copy with matching UUIDs. Geometry and asset references are shared
        // values, so the copy points at the same mesh data and the same material asset.
        Ref<Scene> Clone() const;

        Entity GetEntityByUUID(UUID uuid);
        // Resolves the key a physics query returns. A key that no longer names a live entity - the
        // entity was destroyed after the query ran - resolves to an empty Entity rather than a
        // stale handle.
        Entity GetEntityByKey(uint64_t entityKey);
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
        //
        // The clock is passed in rather than reached for globally: the fixed step the simulation
        // runs at is the scene's PhysicsSettings::FixedTimeStep, and a test can hand in its own
        // Time to drive frame-by-frame without touching the process-wide instance.
        void OnRuntimeStart(Time& clock = Time::Get());
        void OnRuntimeStop();
        bool IsRuntimeRunning() const { return m_RuntimeRunning; }
        // Advances every attached system, phase by phase.
        //
        // The delta is the frame's *unscaled* time. While the runtime is running the fixed update
        // phase runs once per owed fixed step, the update phase receives the time-scaled delta, and
        // rendering interpolates by GetInterpolationAlpha(). A scene that is not running keeps the
        // plain behaviour the editor relies on: one pass per phase, no fixed steps at all.
        void OnUpdate(Timestep unscaledDelta);
        void Step();
        Time& GetTime() { return *m_Clock; }
        const Time& GetTime() const { return *m_Clock; }
        ScriptCommandQueue& GetScriptCommands() { return m_ScriptCommands; }
        void SetInputMap(const InputMap* input) { m_InputMap = input; }
        const InputMap* GetInputMap() const { return m_InputMap; }

        // Fixed step the runtime is stepping at. Zero while the runtime is stopped.
        float GetFixedDeltaTime() const;
        // How far the frame sits between the last simulated state and the next one, in [0, 1).
        // Exactly 1 when the scene is not running, which makes interpolation a no-op.
        float GetInterpolationAlpha() const;
        // Drops the time owed to the fixed update phase. Called when the runtime starts and stops,
        // so a scene never resumes with a step debt from a previous run.
        void ResetFixedStep();

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

        // Whether the in-memory scene differs from the file it was loaded from or last saved to.
        // Clearing is the serializer's job, because only it knows that a write succeeded; marking
        // is the caller's, and in the editor every edit path goes through CommandHistory.
        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }
        void ClearDirty() { m_Dirty = false; }

        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        EnvironmentSettings& GetEnvironmentSettings() { return m_Environment; }
        const EnvironmentSettings& GetEnvironmentSettings() const { return m_Environment; }

        PhysicsSettings& GetPhysicsSettings() { return m_PhysicsSettings; }
        const PhysicsSettings& GetPhysicsSettings() const { return m_PhysicsSettings; }

        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }
        const std::unordered_map<UUID, entt::entity>& GetEntityMap() const { return m_EntityMap; }

        // Where simulation events are published. Systems and the script layer subscribe, and the
        // scene owns the dispatcher so that neither has to know about the other.
        //
        // Events are triggered immediately, from inside the phase that produced them: a physics
        // event reaches its handlers before the update phase runs, which is what lets a handler
        // trust that nothing has moved since the step it is being told about.
        entt::dispatcher& GetDispatcher() { return m_Dispatcher; }
        const entt::dispatcher& GetDispatcher() const { return m_Dispatcher; }

    private:
        void DestroyChildren(entt::entity entity);
        void CollectLights(LightingEnvironment& outLighting) const;

        // Runs every system of one phase. Scene::OnUpdate calls it once per phase, and once more per
        // fixed step.
        void RunPhase(ScenePhase phase, Timestep timestep);

        // Composes the parent chain, guarding against a malformed or over-long one.
        Math::Mat4 GetWorldTransformRecursive(entt::entity entity, size_t depth) const;
        // Same, for a local transform that differs from the stored one - the interpolated one.
        Math::Mat4 GetWorldTransformRecursive(entt::entity entity, const Math::Mat4& localTransform, size_t depth) const;

        // World transform a render item uses: the stored one, or the blend between the last two
        // simulated states for a dynamic rigid body that has been stepped.
        Math::Mat4 GetInterpolatedWorldTransform(entt::entity entity, const TransformComponent& transform) const;
        Math::Mat4 GetWorldTransform(entt::entity entity, const TransformComponent& transform) const;

        // True when making parentUUID the parent of child would close a loop.
        bool WouldCreateCycle(entt::entity child, UUID parentUUID) const;

        void RebuildChildrenIndexIfDirty();
        UUID GetEntityUUID(entt::entity entity) const;

        // Inserts by phase, keeping insertion order inside a phase.
        SceneSystem& AddSystemImpl(Scope<SceneSystem> system);

    private:
        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityMap;
        entt::dispatcher m_Dispatcher;
        ScriptCommandQueue m_ScriptCommands;
        const InputMap* m_InputMap = nullptr;

        // Derived from RelationshipComponent::Parent. Rebuilt on demand after any hierarchy
        // change rather than mirrored on every edit.
        std::unordered_map<UUID, std::vector<UUID>> m_ChildrenIndex;
        bool m_ChildrenIndexDirty = false;

        std::string m_Name = "Untitled Scene";
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;
        EnvironmentSettings m_Environment;
        PhysicsSettings m_PhysicsSettings;
        bool m_Dirty = false;

        std::vector<Scope<SceneSystem>> m_Systems;
        // The systems OnRuntimeStart attached, so stopping detaches exactly those.
        std::vector<SceneSystem*> m_RuntimeSystems;
        bool m_RuntimeRunning = false;
        bool m_RenderCurrentTransform = false;

        // Frame clock the fixed update phase consumes steps from. Only read while running.
        Time* m_Clock = &Time::Get();

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
