#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Core/UUID.h"
#include "Physics/PhysicsWorld.h"
#include "Renderer/EditorCamera.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/RenderView.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
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

        Math::Mat4 GetWorldTransform(entt::entity entity) const;

        void SetViewportSize(uint32_t width, uint32_t height);
        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnUpdate(Timestep timestep);

        // Extracts everything a renderer needs for one frame, as plain data. Rendering
        // itself is the caller's job, which is why the editor viewport, the game panel
        // and the Player can share this without sharing renderer state.
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
        bool IsPhysicsRunning() const { return m_PhysicsWorld != nullptr && m_PhysicsWorld->IsRunning(); }
        bool IsScriptRunning() const { return m_ScriptWorld != nullptr && m_ScriptWorld->IsRunning(); }

        entt::registry& GetRegistry() { return m_Registry; }
        const std::unordered_map<UUID, entt::entity>& GetEntityMap() const { return m_EntityMap; }

    private:
        void DestroyChildren(entt::entity entity);
        void CollectLights(LightingEnvironment& outLighting) const;

    private:
        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityMap;

        std::string m_Name = "Untitled Scene";
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;
        EnvironmentSettings m_Environment;
        PhysicsSettings m_PhysicsSettings;
        Scope<PhysicsWorld> m_PhysicsWorld;
        Scope<ScriptWorld> m_ScriptWorld;

        friend class Entity;
        friend class SceneSerializer;
    };
}
