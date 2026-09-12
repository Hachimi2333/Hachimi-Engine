#include "Scene/Scene.h"

#include "Core/Log.h"
#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

#include <utility>

namespace HachimiEngine
{
    Scene::Scene()
    {
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
        Entity entity(m_Registry.create(), this);
        entity.AddComponent<IDComponent>().ID = UUID();
        entity.AddComponent<TagComponent>().Tag = name.empty() ? "Entity" : name;
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        m_EntityMap[entity.GetUUID()] = entity.GetHandle();
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity)
        {
            return;
        }

        DestroyChildren(entity.GetHandle());

        if (m_PhysicsWorld != nullptr)
        {
            m_PhysicsWorld->DestroyBody(static_cast<uint64_t>(entt::to_integral(entity.GetHandle())));
        }

        m_EntityMap.erase(entity.GetUUID());
        m_Registry.destroy(entity.GetHandle());
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        if (!entity)
        {
            return {};
        }

        const std::string name = entity.GetName();
        Entity duplicate = CreateEntity(name + " Copy");

        if (entity.HasComponent<TransformComponent>())
        {
            duplicate.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();
        }
        else
        {
            duplicate.RemoveComponent<TransformComponent>();
        }

        if (entity.HasComponent<MeshComponent>())
        {
            duplicate.AddComponent<MeshComponent>() = entity.GetComponent<MeshComponent>();
        }
        if (entity.HasComponent<CameraComponent>())
        {
            auto& camera = duplicate.AddComponent<CameraComponent>();
            camera = entity.GetComponent<CameraComponent>();
            camera.Primary = false;
        }
        if (entity.HasComponent<LightComponent>())
        {
            duplicate.AddComponent<LightComponent>() = entity.GetComponent<LightComponent>();
        }
        if (entity.HasComponent<RigidbodyComponent>())
        {
            duplicate.AddComponent<RigidbodyComponent>() = entity.GetComponent<RigidbodyComponent>();
        }
        if (entity.HasComponent<ColliderComponent>())
        {
            duplicate.AddComponent<ColliderComponent>() = entity.GetComponent<ColliderComponent>();
        }
        if (entity.HasComponent<ScriptComponent>())
        {
            duplicate.AddComponent<ScriptComponent>() = entity.GetComponent<ScriptComponent>();
        }

        return duplicate;
    }

    Ref<Scene> Scene::Clone() const
    {
        Ref<Scene> clone = CreateRef<Scene>();

        // The Scene constructor creates a default environment; discard it before copying.
        clone->m_Registry.clear();
        clone->m_EntityMap.clear();

        clone->m_Name = m_Name;
        clone->m_ViewportWidth = m_ViewportWidth;
        clone->m_ViewportHeight = m_ViewportHeight;
        clone->m_Environment = m_Environment;
        clone->m_PhysicsSettings = m_PhysicsSettings;

        const auto entities = m_Registry.view<IDComponent>();
        for (const entt::entity sourceHandle : entities)
        {
            Entity targetEntity(clone->m_Registry.create(), clone.get());
            targetEntity.AddComponent<IDComponent>() = m_Registry.get<IDComponent>(sourceHandle);

            if (const auto* sourceTag = m_Registry.try_get<TagComponent>(sourceHandle))
            {
                targetEntity.AddComponent<TagComponent>() = *sourceTag;
            }
            if (const auto* sourceTransform = m_Registry.try_get<TransformComponent>(sourceHandle))
            {
                targetEntity.AddComponent<TransformComponent>() = *sourceTransform;
            }
            if (const auto* sourceRelationship = m_Registry.try_get<RelationshipComponent>(sourceHandle))
            {
                targetEntity.AddComponent<RelationshipComponent>() = *sourceRelationship;
            }

            if (const auto* sourceMesh = m_Registry.try_get<MeshComponent>(sourceHandle))
            {
                auto& targetMesh = targetEntity.AddComponent<MeshComponent>();
                targetMesh = *sourceMesh;

                // Clone the material override so runtime edits do not affect the editor scene.
                if (sourceMesh->MaterialOverride != nullptr)
                {
                    const Ref<Material>& sourceMaterial = sourceMesh->MaterialOverride;
                    targetMesh.MaterialOverride = Material::Create(sourceMaterial->GetShader());
                    targetMesh.MaterialOverride->SetAlbedoTexture(sourceMaterial->GetAlbedoTexture());
                    targetMesh.MaterialOverride->SetAlbedoColor(sourceMaterial->GetAlbedoColor());
                    targetMesh.MaterialOverride->SetRoughness(sourceMaterial->GetRoughness());
                    targetMesh.MaterialOverride->SetMetallic(sourceMaterial->GetMetallic());
                }
            }

            if (const auto* sourceCamera = m_Registry.try_get<CameraComponent>(sourceHandle))
            {
                targetEntity.AddComponent<CameraComponent>() = *sourceCamera;
            }

            if (const auto* sourceLight = m_Registry.try_get<LightComponent>(sourceHandle))
            {
                targetEntity.AddComponent<LightComponent>() = *sourceLight;
            }

            if (const auto* sourceRigidbody = m_Registry.try_get<RigidbodyComponent>(sourceHandle))
            {
                targetEntity.AddComponent<RigidbodyComponent>() = *sourceRigidbody;
            }

            if (const auto* sourceCollider = m_Registry.try_get<ColliderComponent>(sourceHandle))
            {
                targetEntity.AddComponent<ColliderComponent>() = *sourceCollider;
            }

            if (const auto* sourceScript = m_Registry.try_get<ScriptComponent>(sourceHandle))
            {
                targetEntity.AddComponent<ScriptComponent>() = *sourceScript;
            }

            clone->m_EntityMap[targetEntity.GetUUID()] = targetEntity.GetHandle();
        }

        return clone;
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
        if (m_PhysicsWorld != nullptr || m_ScriptWorld != nullptr)
        {
            HE_CORE_WARN("Scene runtime is already running");
            return;
        }

        m_PhysicsWorld = CreateScope<PhysicsWorld>(m_PhysicsSettings);
        if (!m_PhysicsWorld->IsRunning())
        {
            m_PhysicsWorld = nullptr;
            return;
        }

        m_PhysicsWorld->CreateBodies(*this);

        m_ScriptWorld = CreateScope<ScriptWorld>();
        m_ScriptWorld->OnRuntimeStart(*this);
    }

    void Scene::OnRuntimeStop()
    {
        // Destroy scripts before physics so OnDestroy callbacks can still query
        // the physics world during teardown.
        if (m_ScriptWorld != nullptr)
        {
            m_ScriptWorld->OnRuntimeStop(*this);
            m_ScriptWorld = nullptr;
        }

        m_PhysicsWorld = nullptr;
    }

    void Scene::OnUpdate(Timestep timestep)
    {
        if (m_PhysicsWorld == nullptr)
        {
            return;
        }

        m_PhysicsWorld->Update(*this, timestep);

        if (m_ScriptWorld != nullptr)
        {
            m_ScriptWorld->OnUpdate(timestep, *this);
        }
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
            // The shader declares a fixed point light array, so the extras cannot be lit.
            // Say so instead of silently rendering a darker scene.
            HE_CORE_WARN("Scene '{}' has {} point light(s) beyond the {} the renderer supports; the extras are not lit",
                m_Name,
                droppedPointLights,
                LightingEnvironment::MaxPointLights);
        }

        if (!hasAnyLight)
        {
            // A completely dark scene is not useful for editing; restore the default
            // environment only when the user did not add any lights.
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
        auto* relationship = m_Registry.try_get<RelationshipComponent>(entity);
        if (relationship == nullptr)
        {
            return;
        }

        for (const UUID childUUID : relationship->Children)
        {
            const auto childIt = m_EntityMap.find(childUUID);
            if (childIt != m_EntityMap.end())
            {
                DestroyEntity(Entity(childIt->second, this));
            }
        }
    }
}
