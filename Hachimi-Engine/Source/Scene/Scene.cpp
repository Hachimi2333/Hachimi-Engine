#include "Scene/Scene.h"

#include "Core/Log.h"
#include "Renderer/Lighting.h"
#include "Renderer/MeshFactory.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Scene/Systems/ScriptSystem.h"
#include "Math/Math.h"

#include <algorithm>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        // Guards the parent walk against a malformed chain as well as an accidental loop.
        constexpr size_t MaxHierarchyDepth = 256;
    }

    Scene::Scene()
    {
        ComponentRegistry::EnsureBuiltinComponentsRegistered();

        // A default scene contains a camera, a light, and one visible cube.
        Entity cameraEntity = CreateEntity("Camera");
        cameraEntity.AddComponent<CameraComponent>().Primary = true;
        cameraEntity.Transform().Position = { 0.0f, 3.0f, 8.0f };
        cameraEntity.Transform().Rotation = { 0.0f, -20.0f, 0.0f };

        Entity lightEntity = CreateEntity("Point Light");
        auto& light = lightEntity.AddComponent<LightComponent>();
        light.Type = LightComponent::LightType::Point;
        light.Intensity = 14.0f;
        lightEntity.Transform().Position = { 3.0f, 4.0f, 2.0f };

        Entity cubeEntity = CreateEntity("Cube");
        auto& mesh = cubeEntity.AddComponent<MeshComponent>();
        mesh.PrimitiveType = PrimitiveMeshType::Cube;
        mesh.Mesh = MeshFactory::CreateCube();
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        const entt::entity handle = m_Registry.create();

        // Required components come from the registry, so "every entity has an ID, a tag, a
        // transform and a place in the hierarchy" is stated once.
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.Required)
            {
                descriptor.AddDefault(m_Registry, handle);
            }
        }

        Entity entity(handle, this);
        entity.GetComponent<TagComponent>().Tag = name.empty() ? "Entity" : name;
        m_EntityMap[entity.GetUUID()] = handle;
        m_ChildrenIndexDirty = true;
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity)
        {
            return;
        }

        DestroyChildren(entity.GetHandle());

        // The physics body lives with the system that owns the world.
        if (PhysicsSystem* physics = FindSystem<PhysicsSystem>())
        {
            if (PhysicsWorld* world = physics->GetWorld())
            {
                world->DestroyBody(static_cast<uint64_t>(entt::to_integral(entity.GetHandle())));
            }
        }

        m_EntityMap.erase(entity.GetUUID());
        // Anything that pointed at this entity now points at nothing, so the derived index has
        // to be rebuilt before the next hierarchy query.
        m_ChildrenIndexDirty = true;
        m_Registry.destroy(entity.GetHandle());
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        if (!entity)
        {
            return {};
        }

        Entity duplicate = CreateEntity(entity.GetName() + " Copy");

        // Descriptors decide what a duplicate inherits: the UUID and the hierarchy placement
        // belong to the source entity, everything else is copied.
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (!descriptor.CopiedOnDuplicate)
            {
                continue;
            }

            descriptor.Clone(m_Registry, entity.GetHandle(), m_Registry, duplicate.GetHandle());
        }

        m_ChildrenIndexDirty = true;
        return duplicate;
    }

    Ref<Scene> Scene::Clone() const
    {
        Ref<Scene> clone = CreateRef<Scene>();

        // The Scene constructor creates a default environment; discard it before copying.
        clone->m_Registry.clear();
        clone->m_EntityMap.clear();
        clone->m_ChildrenIndex.clear();
        clone->m_ChildrenIndexDirty = true;

        clone->m_Name = m_Name;
        clone->m_ViewportWidth = m_ViewportWidth;
        clone->m_ViewportHeight = m_ViewportHeight;
        clone->m_Environment = m_Environment;
        clone->m_PhysicsSettings = m_PhysicsSettings;

        auto idView = m_Registry.view<IDComponent>();
        for (const entt::entity sourceHandle : idView)
        {
            const entt::entity targetHandle = clone->m_Registry.create();

            for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
            {
                descriptor.Clone(m_Registry, sourceHandle, clone->m_Registry, targetHandle);
            }

            clone->m_EntityMap[clone->m_Registry.get<IDComponent>(targetHandle).ID] = targetHandle;
        }

        return clone;
    }

    void Scene::SetParent(Entity child, Entity parent)
    {
        if (!child || !parent)
        {
            return;
        }

        if (child == parent)
        {
            HE_CORE_WARN("An entity cannot be its own parent");
            return;
        }

        if (WouldCreateCycle(child.GetHandle(), parent.GetUUID()))
        {
            HE_CORE_WARN("Reparenting '{}' under '{}' would create a cycle; the parent is unchanged",
                child.GetName(),
                parent.GetName());
            return;
        }

        ClearParent(child);
        child.GetComponent<RelationshipComponent>().Parent = parent.GetUUID();
        m_ChildrenIndexDirty = true;
    }

    void Scene::ClearParent(Entity child)
    {
        if (!child || !child.HasComponent<RelationshipComponent>())
        {
            return;
        }

        child.GetComponent<RelationshipComponent>().Parent = UUID::Invalid();
        m_ChildrenIndexDirty = true;
    }

    std::vector<Entity> Scene::GetChildren(Entity parent)
    {
        std::vector<Entity> children;
        if (!parent)
        {
            return children;
        }

        RebuildChildrenIndexIfDirty();

        const auto indexIt = m_ChildrenIndex.find(parent.GetUUID());
        if (indexIt == m_ChildrenIndex.end())
        {
            return children;
        }

        children.reserve(indexIt->second.size());
        for (const UUID childId : indexIt->second)
        {
            const auto childIt = m_EntityMap.find(childId);
            if (childIt != m_EntityMap.end())
            {
                children.emplace_back(childIt->second, this);
            }
        }

        return children;
    }

    bool Scene::IsAncestorOf(Entity ancestor, Entity candidate)
    {
        if (!ancestor || !candidate || ancestor == candidate)
        {
            return false;
        }

        const UUID ancestorId = ancestor.GetUUID();
        UUID current = candidate.GetComponent<RelationshipComponent>().Parent;

        for (size_t depth = 0; current != UUID::Invalid() && depth < MaxHierarchyDepth; ++depth)
        {
            if (current == ancestorId)
            {
                return true;
            }

            const auto parentIt = m_EntityMap.find(current);
            if (parentIt == m_EntityMap.end())
            {
                return false;
            }

            const auto* relationship = m_Registry.try_get<RelationshipComponent>(parentIt->second);
            current = relationship != nullptr ? relationship->Parent : UUID::Invalid();
        }

        return false;
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        const auto it = m_EntityMap.find(uuid);
        if (it == m_EntityMap.end())
        {
            return {};
        }
        return Entity(it->second, this);
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        auto view = m_Registry.view<CameraComponent>();
        for (const entt::entity entity : view)
        {
            if (view.get<CameraComponent>(entity).Primary)
            {
                return Entity(entity, this);
            }
        }
        return {};
    }

    std::vector<Entity> Scene::GetAllEntities()
    {
        std::vector<Entity> entities;
        entities.reserve(m_EntityMap.size());

        auto view = m_Registry.view<IDComponent>();
        for (const entt::entity entity : view)
        {
            entities.emplace_back(entity, this);
        }
        return entities;
    }

    Math::Mat4 Scene::GetWorldTransform(entt::entity entity) const
    {
        Math::Mat4 transform = m_Registry.get<TransformComponent>(entity).GetTransform();

        const auto* relationship = m_Registry.try_get<RelationshipComponent>(entity);
        if (relationship != nullptr && relationship->Parent != UUID::Invalid())
        {
            const auto parentIt = m_EntityMap.find(relationship->Parent);
            if (parentIt != m_EntityMap.end())
            {
                transform = GetWorldTransform(parentIt->second) * transform;
            }
        }

        return transform;
    }

    void Scene::SetViewportSize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
    }

    void Scene::OnRuntimeStart()
    {
        if (m_RuntimeRunning)
        {
            HE_CORE_WARN("Scene runtime is already running");
            return;
        }

        m_RuntimeRunning = true;

        // Physics runs in the fixed update phase and scripts in the variable update phase, so the
        // order between them comes from the phases rather than from the order of these two lines.
        m_RuntimeSystems.push_back(&AddSystem<PhysicsSystem>());
        m_RuntimeSystems.push_back(&AddSystem<ScriptSystem>());
    }

    void Scene::OnRuntimeStop()
    {
        // Detaching in reverse keeps teardown symmetric with startup, and scripts are torn down
        // before physics so OnDestroy callbacks can still query the physics world.
        for (auto it = m_RuntimeSystems.rbegin(); it != m_RuntimeSystems.rend(); ++it)
        {
            RemoveSystem(**it);
        }

        m_RuntimeSystems.clear();
        m_RuntimeRunning = false;
    }

    void Scene::OnUpdate(Timestep timestep)
    {
        for (const ScenePhase phase : { ScenePhase::PreUpdate, ScenePhase::FixedUpdate, ScenePhase::Update, ScenePhase::LateUpdate })
        {
            for (const Scope<SceneSystem>& system : m_Systems)
            {
                if (system != nullptr && system->GetPhase() == phase)
                {
                    system->OnUpdate(*this, timestep);
                }
            }
        }
    }

    SceneSystem& Scene::AddSystemImpl(Scope<SceneSystem> system)
    {
        HE_CORE_ASSERT(system != nullptr);

        SceneSystem& reference = *system;
        reference.OnAttach(*this);

        // Insert after every system of the same or an earlier phase, so phases stay ordered and
        // insertion order is kept inside a phase.
        const auto insertPosition = std::find_if(
            m_Systems.begin(),
            m_Systems.end(),
            [&reference](const Scope<SceneSystem>& candidate)
            {
                return candidate == nullptr || candidate->GetPhase() > reference.GetPhase();
            });

        m_Systems.insert(insertPosition, std::move(system));
        return reference;
    }

    void Scene::RemoveSystem(SceneSystem& system)
    {
        const auto position = std::find_if(
            m_Systems.begin(),
            m_Systems.end(),
            [&system](const Scope<SceneSystem>& candidate) { return candidate.get() == &system; });

        if (position == m_Systems.end())
        {
            return;
        }

        // Detach before releasing, so the system can still reach the scene it was attached to.
        (*position)->OnDetach(*this);
        m_Systems.erase(position);
    }

    bool Scene::IsPhysicsRunning() const
    {
        const PhysicsSystem* physics = FindSystem<PhysicsSystem>();
        return physics != nullptr && physics->IsRunning();
    }

    bool Scene::IsScriptRunning() const
    {
        const ScriptSystem* scripts = FindSystem<ScriptSystem>();
        return scripts != nullptr && scripts->IsRunning();
    }

    RenderView Scene::BuildRenderView(const EditorCamera& camera, bool drawGrid) const
    {
        SceneRenderDesc desc;
        desc.View = camera.GetViewMatrix();
        desc.Projection = camera.GetProjection();
        desc.CameraPosition = camera.GetPosition();
        desc.DrawGrid = drawGrid;
        return BuildRenderView(desc);
    }

    RenderView Scene::BuildRenderView(const SceneRenderDesc& desc) const
    {
        RenderView view;

        view.View = desc.View;
        view.Projection = desc.Projection;
        view.ViewProjection = desc.Projection * desc.View;
        view.CameraPosition = desc.CameraPosition;
        view.CameraForward = Math::Normalize(-Math::Vec3(Math::Transpose(desc.View)[2]));
        view.Environment = m_Environment;
        view.DrawGrid = desc.DrawGrid;

        CollectLights(view.Lighting);

        auto meshView = m_Registry.view<MeshComponent, TransformComponent>();
        view.Items.reserve(meshView.size_hint());

        for (const entt::entity entity : meshView)
        {
            const auto& [meshComponent, transformComponent] = meshView.get<MeshComponent, TransformComponent>(entity);
            if (!meshComponent.Visible || meshComponent.Mesh == nullptr)
            {
                continue;
            }

            RenderItem item;
            item.Mesh = meshComponent.Mesh;
            item.Transform = GetWorldTransform(entity);
            item.AlbedoColor = meshComponent.MaterialColor;
            item.Roughness = meshComponent.Roughness;
            item.Metallic = meshComponent.Metallic;
            item.Material = meshComponent.MaterialOverride;
            view.Items.push_back(std::move(item));
        }

        return view;
    }

    void Scene::CollectLights(LightingEnvironment& outLighting) const
    {
        LightingEnvironment lighting;
        lighting.Directional.Direction = Math::Vec3(0.0f);
        lighting.Directional.Color = Math::Vec3(0.0f);
        lighting.Directional.Intensity = 0.0f;
        lighting.PointLightCount = 0;

        for (PointLight& pointLight : lighting.PointLights)
        {
            pointLight.Position = Math::Vec3(0.0f);
            pointLight.Color = Math::Vec3(0.0f);
            pointLight.Intensity = 0.0f;
            pointLight.Range = 0.0f;
        }

        bool hasAnyLight = false;
        bool hasDirectionalLight = false;
        size_t droppedPointLights = 0;

        auto lightView = m_Registry.view<LightComponent, TransformComponent>();
        for (const entt::entity entity : lightView)
        {
            const auto& [lightComponent, transformComponent] = lightView.get<LightComponent, TransformComponent>(entity);
            hasAnyLight = true;

            if (lightComponent.Type == LightComponent::LightType::Directional)
            {
                hasDirectionalLight = true;
                lighting.Directional.Color = lightComponent.Color;
                lighting.Directional.Intensity = lightComponent.Intensity;
                lighting.Directional.CastsShadows = lightComponent.CastsShadows;
                lighting.Directional.ShadowBias = lightComponent.ShadowBias;

                const Math::Quat rotation = Math::Quat(Math::Radians(transformComponent.Rotation));
                lighting.Directional.Direction = Math::Normalize(Math::Rotate(rotation, Math::Vec3(0.0f, 0.0f, -1.0f)));
            }
            else if (lighting.PointLightCount < static_cast<int>(lighting.PointLights.size()))
            {
                PointLight& pointLight = lighting.PointLights[static_cast<size_t>(lighting.PointLightCount++)];
                pointLight.Position = GetWorldTransform(entity) * Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
                pointLight.Color = lightComponent.Color;
                pointLight.Intensity = lightComponent.Intensity;
                pointLight.Range = lightComponent.Range;
            }
            else
            {
                ++droppedPointLights;
            }
        }

        if (droppedPointLights > 0)
        {
            // The shader declares a fixed point light array, so the extras cannot be lit. Say so
            // instead of silently rendering a darker scene.
            HE_CORE_WARN("Scene '{}' has {} point light(s) beyond the {} the renderer supports; the extras are not lit",
                m_Name,
                droppedPointLights,
                LightingEnvironment::MaxPointLights);
        }

        if (!hasAnyLight)
        {
            // A completely dark scene is not useful for editing; restore the default environment
            // only when the user did not add any lights.
            lighting = LightingEnvironment();
        }
        else if (!hasDirectionalLight)
        {
            // Do not apply a phantom directional light when the scene only has point lights.
            lighting.Directional.Intensity = 0.0f;
        }

        outLighting = lighting;
    }

    void Scene::DestroyChildren(entt::entity entity)
    {
        RebuildChildrenIndexIfDirty();

        const auto indexIt = m_ChildrenIndex.find(GetEntityUUID(entity));
        if (indexIt == m_ChildrenIndex.end())
        {
            return;
        }

        // Snapshot: destroying a child rebuilds the index underneath the loop.
        const std::vector<UUID> childIds = indexIt->second;
        for (const UUID childId : childIds)
        {
            const auto childIt = m_EntityMap.find(childId);
            if (childIt != m_EntityMap.end())
            {
                DestroyEntity(Entity(childIt->second, this));
            }
        }
    }

    bool Scene::WouldCreateCycle(entt::entity child, UUID parentUUID) const
    {
        const UUID childId = GetEntityUUID(child);
        UUID current = parentUUID;

        for (size_t depth = 0; current != UUID::Invalid() && depth < MaxHierarchyDepth; ++depth)
        {
            if (current == childId)
            {
                return true;
            }

            const auto parentIt = m_EntityMap.find(current);
            if (parentIt == m_EntityMap.end())
            {
                return false;
            }

            const auto* relationship = m_Registry.try_get<RelationshipComponent>(parentIt->second);
            current = relationship != nullptr ? relationship->Parent : UUID::Invalid();
        }

        // Either the chain ended, or it is longer than any real hierarchy and is treated as
        // broken rather than trusted.
        return current != UUID::Invalid();
    }

    void Scene::RebuildChildrenIndexIfDirty()
    {
        if (!m_ChildrenIndexDirty)
        {
            return;
        }

        m_ChildrenIndex.clear();

        auto view = m_Registry.view<RelationshipComponent, IDComponent>();
        for (const entt::entity entity : view)
        {
            const UUID parent = view.get<RelationshipComponent>(entity).Parent;
            if (parent != UUID::Invalid())
            {
                m_ChildrenIndex[parent].push_back(view.get<IDComponent>(entity).ID);
            }
        }

        m_ChildrenIndexDirty = false;
    }

    UUID Scene::GetEntityUUID(entt::entity entity) const
    {
        const auto* identity = m_Registry.try_get<IDComponent>(entity);
        return identity != nullptr ? identity->ID : UUID::Invalid();
    }
}
