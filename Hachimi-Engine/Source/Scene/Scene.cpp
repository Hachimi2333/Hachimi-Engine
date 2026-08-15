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
        RenderScene(camera.GetViewMatrix(), camera.GetProjection(), camera.GetPosition(), true);
    }

    void Scene::OnRender(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition)
    {
        RenderScene(view, projection, cameraPosition, false);
    }

    void Scene::RenderScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, bool drawGrid)
    {
        ApplyLightsToRenderer();
        SceneRenderer::GetEnvironmentSettings() = m_Environment;
        SceneRenderer::BeginScene(view, projection, cameraPosition);

        const LightingEnvironment& lighting = SceneRenderer::GetLightingEnvironment();
        const bool castsDirectionalShadows = lighting.Directional.CastsShadows
            && lighting.Directional.Intensity > 0.0f
            && glm::length(lighting.Directional.Direction) > 0.001f;

        if (castsDirectionalShadows)
        {
            const glm::mat4 lightViewProjection = SceneRenderer::CalculateDirectionalLightViewProjection(cameraPosition);
            SceneRenderer::BeginDirectionalShadowPass(lightViewProjection);

            auto shadowMeshView = m_Registry.view<MeshComponent, TransformComponent>();
            for (const entt::entity entity : shadowMeshView)
            {
                const auto& [meshComponent, transformComponent] = shadowMeshView.get<MeshComponent, TransformComponent>(entity);
                if (!meshComponent.Visible || meshComponent.Mesh == nullptr || meshComponent.Mesh->GetDrawMode() != MeshDrawMode::Triangles)
                {
                    continue;
                }

                SceneRenderer::SubmitShadowMesh(meshComponent.Mesh, GetWorldTransform(entity));
            }

            SceneRenderer::EndDirectionalShadowPass();
        }

        SceneRenderer::DrawSkybox();

        if (drawGrid)
        {
            SceneRenderer::DrawGrid();
        }

        auto meshView = m_Registry.view<MeshComponent, TransformComponent>();
        for (const entt::entity entity : meshView)
        {
            const auto& [meshComponent, transformComponent] = meshView.get<MeshComponent, TransformComponent>(entity);
            if (!meshComponent.Visible || meshComponent.Mesh == nullptr)
            {
                continue;
            }

            const glm::mat4 worldTransform = GetWorldTransform(entity);

            // Keep the per-entity material parameters authoritative even when no
            // explicit material override was created yet.
            if (meshComponent.MaterialOverride == nullptr)
            {
                meshComponent.MaterialOverride = Material::Create(SceneRenderer::GetDefaultMaterial()->GetShader());
            }

            Ref<Material> material = meshComponent.MaterialOverride;
            material->SetAlbedoColor(meshComponent.MaterialColor);
            material->SetRoughness(meshComponent.Roughness);
            material->SetMetallic(meshComponent.Metallic);

            SceneRenderer::SubmitMesh(meshComponent.Mesh, worldTransform, material);
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
        lighting.Directional.Direction = glm::vec3(0.0f);
        lighting.Directional.Color = glm::vec3(0.0f);
        lighting.Directional.Intensity = 0.0f;
        lighting.PointLightCount = 0;

        for (PointLight& pointLight : lighting.PointLights)
        {
            pointLight.Position = glm::vec3(0.0f);
            pointLight.Color = glm::vec3(0.0f);
            pointLight.Intensity = 0.0f;
            pointLight.Range = 0.0f;
        }

        bool hasAnyLight = false;
        bool hasDirectionalLight = false;

        auto view = m_Registry.view<LightComponent, TransformComponent>();
        for (const entt::entity entity : view)
        {
            const auto& [lightComponent, transformComponent] = view.get<LightComponent, TransformComponent>(entity);
            hasAnyLight = true;

            if (lightComponent.Type == LightComponent::LightType::Directional)
            {
                hasDirectionalLight = true;
                lighting.Directional.Color = lightComponent.Color;
                lighting.Directional.Intensity = lightComponent.Intensity;
                lighting.Directional.CastsShadows = lightComponent.CastsShadows;
                lighting.Directional.ShadowBias = lightComponent.ShadowBias;

                const glm::quat rotation = glm::quat(glm::radians(transformComponent.Rotation));
                lighting.Directional.Direction = glm::normalize(glm::rotate(rotation, glm::vec3(0.0f, 0.0f, -1.0f)));
            }
            else if (lighting.PointLightCount < static_cast<int>(lighting.PointLights.size()))
            {
                PointLight& pointLight = lighting.PointLights[static_cast<size_t>(lighting.PointLightCount++)];
                pointLight.Position = GetWorldTransform(entity) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                pointLight.Color = lightComponent.Color;
                pointLight.Intensity = lightComponent.Intensity;
                pointLight.Range = lightComponent.Range;
            }
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

        SceneRenderer::GetLightingEnvironment() = lighting;
    }
}
