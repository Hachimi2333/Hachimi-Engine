#include "Scene/Scene.h"

#include "Core/Log.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/SceneRenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

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
        duplicate.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();

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

        return duplicate;
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

    glm::mat4 Scene::GetWorldTransform(entt::entity entity) const
    {
        glm::mat4 transform = m_Registry.get<TransformComponent>(entity).GetTransform();

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

    void Scene::OnUpdate(Timestep timestep)
    {
        // Scene-level update hook; entity behavior can be added here later.
    }

    void Scene::OnRender(const EditorCamera& camera)
    {
        ApplyLightsToRenderer();

        SceneRenderer::BeginScene(camera);
        SceneRenderer::DrawGrid();

        auto view = m_Registry.view<MeshComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const auto& [meshComponent, transformComponent] = view.get<MeshComponent, TransformComponent>(entity);
            if (!meshComponent.Visible || meshComponent.Mesh == nullptr)
            {
                continue;
            }

            const glm::mat4 worldTransform = GetWorldTransform(entity);
            SceneRenderer::SubmitMesh(meshComponent.Mesh, worldTransform, meshComponent.MaterialOverride);
        }

        SceneRenderer::EndScene();
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

    void Scene::ApplyLightsToRenderer()
    {
        LightingEnvironment lighting;
        lighting.PointLightCount = 0;

        auto view = m_Registry.view<LightComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const auto& [lightComponent, transformComponent] = view.get<LightComponent, TransformComponent>(entity);

            if (lightComponent.Type == LightComponent::LightType::Directional)
            {
                lighting.Directional.Color = lightComponent.Color;
                lighting.Directional.Intensity = lightComponent.Intensity;

                const glm::quat rotation = glm::quat(glm::radians(transformComponent.Rotation));
                lighting.Directional.Direction = glm::normalize(glm::rotate(rotation, glm::vec3(0.0f, 0.0f, -1.0f)));
            }
            else if (lighting.PointLightCount < static_cast<int>(lighting.PointLights.size()))
            {
                PointLight& pointLight = lighting.PointLights[static_cast<size_t>(lighting.PointLightCount++)];
                pointLight.Position = GetWorldTransform(entity) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                pointLight.Color = lightComponent.Color;
                pointLight.Intensity = lightComponent.Intensity;
            }
        }

        if (lighting.PointLightCount == 0)
        {
            lighting.PointLightCount = 1;
        }

        SceneRenderer::GetLightingEnvironment() = lighting;
    }
}
