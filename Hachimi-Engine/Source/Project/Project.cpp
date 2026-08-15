#include "Project/Project.h"

#include "Core/Log.h"
#include "Renderer/MeshFactory.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/FileSystem.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    namespace
    {
        Entity CreateMeshEntity(
            Scene& scene,
            const std::string& name,
            const Ref<Mesh>& mesh,
            PrimitiveMeshType primitiveType,
            const glm::vec3& position,
            const glm::vec3& rotation,
            const glm::vec3& scale,
            const glm::vec4& color,
            float roughness,
            float metallic)
        {
            Entity entity = scene.CreateEntity(name);
            entity.Transform().Position = position;
            entity.Transform().Rotation = rotation;
            entity.Transform().Scale = scale;

            auto& meshComponent = entity.AddComponent<MeshComponent>();
            meshComponent.Mesh = mesh;
            meshComponent.PrimitiveType = primitiveType;
            meshComponent.MaterialColor = color;
            meshComponent.Roughness = roughness;
            meshComponent.Metallic = metallic;
            return entity;
        }

        Entity CreateCubeEntity(
            Scene& scene,
            const std::string& name,
            const glm::vec3& position,
            const glm::vec3& scale,
            const glm::vec4& color,
            float roughness,
            float metallic)
        {
            return CreateMeshEntity(
                scene,
                name,
                MeshFactory::CreateCube(1.0f),
                PrimitiveMeshType::Cube,
                position,
                glm::vec3(0.0f),
                scale,
                color,
                roughness,
                metallic);
        }

        Entity CreateSphereEntity(
            Scene& scene,
            const std::string& name,
            const glm::vec3& position,
            const glm::vec3& scale,
            const glm::vec4& color,
            float roughness,
            float metallic)
        {
            return CreateMeshEntity(
                scene,
                name,
                MeshFactory::CreateSphere(1.0f, 64, 32),
                PrimitiveMeshType::Sphere,
                position,
                glm::vec3(0.0f),
                scale,
                color,
                roughness,
                metallic);
        }

        void ConfigureShowcaseScene(const Ref<Scene>& scene)
        {
            // Replace the minimal constructor scene with a full showcase of the
            // renderer features: PBR materials, shadows, multiple lights and hierarchy.
            const std::vector<Entity> defaultEntities = scene->GetAllEntities();
            for (Entity entity : defaultEntities)
            {
                scene->DestroyEntity(entity);
            }

            scene->SetName("Default Scene");
            EnvironmentSettings& environment = scene->GetEnvironmentSettings();
            environment.ShowSkybox = true;
            environment.Exposure = 1.0f;
            environment.EnvironmentIntensity = 1.0f;

            Entity camera = scene->CreateEntity("Main Camera");
            camera.Transform().Position = { 0.0f, 6.0f, 12.0f };
            camera.Transform().Rotation = { -20.0f, 0.0f, 0.0f };
            auto& cameraComponent = camera.AddComponent<CameraComponent>();
            cameraComponent.Primary = true;
            cameraComponent.FieldOfView = 50.0f;
            cameraComponent.NearClip = 0.1f;
            cameraComponent.FarClip = 500.0f;

            Entity directionalLight = scene->CreateEntity("Sun Light");
            directionalLight.Transform().Rotation = { -50.0f, 30.0f, 0.0f };
            auto& directionalComponent = directionalLight.AddComponent<LightComponent>();
            directionalComponent.Type = LightComponent::LightType::Directional;
            directionalComponent.Color = { 1.0f, 0.96f, 0.88f };
            directionalComponent.Intensity = 2.5f;
            directionalComponent.CastsShadows = true;
            directionalComponent.ShadowBias = 0.0008f;

            Entity warmPointLight = scene->CreateEntity("Warm Point Light");
            warmPointLight.Transform().Position = { -3.0f, 4.0f, 2.5f };
            auto& warmPointComponent = warmPointLight.AddComponent<LightComponent>();
            warmPointComponent.Type = LightComponent::LightType::Point;
            warmPointComponent.Color = { 1.0f, 0.72f, 0.45f };
            warmPointComponent.Intensity = 24.0f;
            warmPointComponent.Range = 15.0f;

            Entity coolPointLight = scene->CreateEntity("Cool Point Light");
            coolPointLight.Transform().Position = { 4.0f, 3.0f, -2.0f };
            auto& coolPointComponent = coolPointLight.AddComponent<LightComponent>();
            coolPointComponent.Type = LightComponent::LightType::Point;
            coolPointComponent.Color = { 0.3f, 0.55f, 1.0f };
            coolPointComponent.Intensity = 12.0f;
            coolPointComponent.Range = 12.0f;

            CreateMeshEntity(
                *scene,
                "Ground",
                MeshFactory::CreatePlane(24.0f, 24.0f),
                PrimitiveMeshType::Plane,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(1.0f),
                { 0.26f, 0.26f, 0.30f, 1.0f },
                0.92f,
                0.0f);

            CreateSphereEntity(*scene, "Polished Metal Sphere", { -4.0f, 1.0f, 1.5f }, glm::vec3(1.0f), { 0.88f, 0.88f, 0.92f, 1.0f }, 0.12f, 1.0f);
            CreateSphereEntity(*scene, "Matte Red Sphere", { -2.0f, 1.0f, 1.5f }, glm::vec3(1.0f), { 0.80f, 0.20f, 0.16f, 1.0f }, 0.88f, 0.0f);
            CreateCubeEntity(*scene, "Glossy Blue Cube", { 0.0f, 1.0f, 1.5f }, glm::vec3(0.9f), { 0.15f, 0.45f, 0.95f, 1.0f }, 0.15f, 0.0f);
            CreateCubeEntity(*scene, "Brass Cube", { 2.0f, 1.0f, 1.5f }, glm::vec3(0.8f), { 1.0f, 0.72f, 0.25f, 1.0f }, 0.35f, 1.0f);
            CreateCubeEntity(*scene, "Purple Pillar", { 4.0f, 1.75f, 1.5f }, glm::vec3(0.9f, 3.5f, 0.9f), { 0.55f, 0.15f, 0.75f, 1.0f }, 0.45f, 0.3f);

            // A parent entity with child meshes demonstrates hierarchy and local transforms.
            Entity cluster = scene->CreateEntity("Crystal Cluster");
            cluster.Transform().Position = { 0.0f, 1.5f, -2.8f };
            auto& clusterRelationship = cluster.GetComponent<RelationshipComponent>();

            Entity childA = CreateCubeEntity(*scene, "Cluster Cube A", { -1.2f, 0.0f, 0.0f }, glm::vec3(0.7f), { 0.90f, 0.30f, 0.20f, 1.0f }, 0.3f, 0.6f);
            childA.GetComponent<RelationshipComponent>().Parent = cluster.GetUUID();
            clusterRelationship.Children.push_back(childA.GetUUID());

            Entity childB = CreateCubeEntity(*scene, "Cluster Cube B", { 1.2f, 0.0f, 0.0f }, glm::vec3(0.7f), { 0.20f, 0.80f, 0.90f, 1.0f }, 0.2f, 1.0f);
            childB.GetComponent<RelationshipComponent>().Parent = cluster.GetUUID();
            clusterRelationship.Children.push_back(childB.GetUUID());

            Entity childC = CreateSphereEntity(*scene, "Cluster Sphere C", { 0.0f, 0.9f, 0.0f }, glm::vec3(0.8f), { 0.30f, 0.90f, 0.35f, 1.0f }, 0.6f, 0.2f);
            childC.GetComponent<RelationshipComponent>().Parent = cluster.GetUUID();
            clusterRelationship.Children.push_back(childC.GetUUID());
        }
    }

    bool Project::OpenScene(const std::filesystem::path& scenePath)
    {
        if (!FileSystem::Exists(scenePath))
        {
            HE_CORE_ERROR("Scene file does not exist: {}", scenePath.string());
            return false;
        }

        const Ref<Scene> scene = CreateRef<Scene>();
        SceneSerializer serializer(scene);
        if (!serializer.Deserialize(scenePath.string()))
        {
            return false;
        }

        m_ActiveScene = scene;
        return true;
    }

    void Project::SaveActiveScene()
    {
        if (m_ActiveScene == nullptr)
        {
            HE_CORE_WARN("Cannot save scene: no active scene");
            return;
        }

        SceneSerializer serializer(m_ActiveScene);
        serializer.Serialize(m_StartScenePath.string());
    }

    Ref<Project> Project::CreateNew(const std::string& name, const std::filesystem::path& directory)
    {
        const std::filesystem::path projectDirectory = directory / name;

        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Meshes");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Textures");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Materials");
        FileSystem::CreateDirectories(projectDirectory / "Assets" / "Scenes");

        const Ref<Scene> defaultScene = CreateRef<Scene>();
        ConfigureShowcaseScene(defaultScene);

        const std::filesystem::path startScenePath = projectDirectory / "Assets" / "Scenes" / "Default.hscene";
        SceneSerializer serializer(defaultScene);
        serializer.Serialize(startScenePath.string());

        const Ref<Project> project = CreateRef<Project>();
        project->m_Name = name;
        project->m_ProjectDirectory = projectDirectory;
        project->m_AssetsDirectory = projectDirectory / "Assets";
        project->m_StartScenePath = startScenePath;
        project->m_ProjectFilePath = projectDirectory / (name + ".hproj");
        project->m_ActiveScene = defaultScene;
        return project;
    }
}
