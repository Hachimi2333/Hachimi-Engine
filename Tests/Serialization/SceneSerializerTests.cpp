// SceneSerializer: .hscene round trips, exercised without a window.
//
// Scenes used to require an OpenGL context because their constructor uploaded a mesh,
// which is why this file did not exist. Geometry is CPU-side now, so the scene format
// is covered headlessly. Files are written into the shared test workspace and read back
// through the virtual file system, which falls back to the disk when nothing is mounted.
//
// Entity, UUID and Math::Vec3 are not streamable, so comparisons go through plain-bool
// helpers instead of raw CHECK(a == b).

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Renderer/MeshData.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Serialization/SceneSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <cmath>
#include <filesystem>
#include <string>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    struct SceneFixture
    {
        std::filesystem::path Directory;
        std::string ScenePath;
    };

    SceneFixture MakeSceneFixture(const std::string& directoryName)
    {
        SceneFixture fixture;
        fixture.Directory = Workspace().PrepareDirectory(directoryName);
        FileSystem::CreateDirectories(fixture.Directory);
        fixture.ScenePath = (fixture.Directory / "RoundTrip.hscene").string();
        return fixture;
    }

    bool SameUUID(UUID lhs, UUID rhs)
    {
        return lhs == rhs;
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    Entity FindEntityByName(const Ref<Scene>& scene, const std::string& name)
    {
        for (const Entity entity : scene->GetAllEntities())
        {
            if (entity.GetName() == name)
            {
                return entity;
            }
        }
        return Entity {};
    }

    // A scene that touches every component the serializer knows about.
    Ref<Scene> MakePopulatedScene()
    {
        const Ref<Scene> scene = CreateRef<Scene>();
        for (const Entity entity : scene->GetAllEntities())
        {
            scene->DestroyEntity(entity);
        }

        scene->SetName("Round Trip Scene");
        scene->GetEnvironmentSettings().ShowSkybox = false;
        scene->GetEnvironmentSettings().Exposure = 1.25f;
        scene->GetEnvironmentSettings().EnvironmentIntensity = 0.5f;
        scene->GetPhysicsSettings().Gravity = { 0.0f, -12.5f, 0.0f };
        scene->GetPhysicsSettings().SubStepCount = 2;

        Entity parent = scene->CreateEntity("Parent");
        parent.Transform().Position = { 1.0f, 2.0f, 3.0f };

        Entity child = scene->CreateEntity("Child");
        child.Transform().Position = { -1.0f, 0.5f, 0.25f };
        child.Transform().Rotation = { 0.0f, 45.0f, 0.0f };
        child.Transform().Scale = { 2.0f, 2.0f, 2.0f };
        scene->SetParent(child, parent);

        auto& mesh = child.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);
        mesh.Primitive = PrimitiveMeshType::Sphere;
        mesh.Mesh = MeshFactory::CreateSphere();
        mesh.AlbedoColor = { 0.1f, 0.2f, 0.3f, 0.4f };
        mesh.Roughness = 0.25f;
        mesh.Metallic = 0.75f;
        mesh.Visible = false;

        auto& rigidbody = child.AddComponent<RigidbodyComponent>();
        rigidbody.Type = RigidbodyComponent::RigidbodyType::Kinematic;
        rigidbody.LinearVelocity = { 0.5f, 0.0f, 0.0f };
        rigidbody.GravityScale = 0.5f;
        rigidbody.IsBullet = true;

        auto& collider = child.AddComponent<ColliderComponent>();
        collider.ShapeType = ColliderComponent::ColliderShapeType::Capsule;
        collider.Radius = 0.4f;
        collider.Height = 1.8f;
        collider.IsTrigger = true;
        collider.CategoryBits = 0b1010ull;

        Entity camera = scene->CreateEntity("Camera");
        auto& cameraComponent = camera.AddComponent<CameraComponent>();
        cameraComponent.Primary = true;
        cameraComponent.FieldOfView = 65.0f;
        cameraComponent.NearClip = 0.2f;
        cameraComponent.FarClip = 750.0f;

        Entity light = scene->CreateEntity("Sun");
        auto& lightComponent = light.AddComponent<LightComponent>();
        lightComponent.Type = LightComponent::LightType::Directional;
        lightComponent.Color = { 1.0f, 0.9f, 0.8f };
        lightComponent.Intensity = 4.0f;
        lightComponent.CastsShadows = false;

        auto& scriptComponent = light.AddComponent<ScriptComponent>();
        ScriptComponent::ScriptReference rotator;
        rotator.DisplayName = "Rotator.lua";
        rotator.Enabled = true;
        scriptComponent.Scripts.push_back(rotator);

        ScriptComponent::ScriptReference disabled;
        disabled.DisplayName = "Disabled.lua";
        disabled.Enabled = false;
        scriptComponent.Scripts.push_back(disabled);

        return scene;
    }
}

TEST_SUITE("Serialization")
{
    TEST_CASE("a scene file round trips through disk without a window")
    {
        const SceneFixture fixture = MakeSceneFixture("SceneRoundTrip");
        const Ref<Scene> source = MakePopulatedScene();

        SceneSerializer writer(source);
        writer.Serialize(fixture.ScenePath);
        REQUIRE(FileSystem::Exists(fixture.ScenePath));

        const Ref<Scene> loaded = CreateRef<Scene>();
        SceneSerializer reader(loaded);
        REQUIRE(reader.Deserialize(fixture.ScenePath));

        CHECK(loaded->GetName() == "Round Trip Scene");
        CHECK(loaded->GetAllEntities().size() == 4);

        CHECK_FALSE(loaded->GetEnvironmentSettings().ShowSkybox);
        CHECK(Near(loaded->GetEnvironmentSettings().Exposure, 1.25f));
        CHECK(Near(loaded->GetEnvironmentSettings().EnvironmentIntensity, 0.5f));
        CHECK(Near(loaded->GetPhysicsSettings().Gravity.y, -12.5f));
        CHECK(loaded->GetPhysicsSettings().SubStepCount == 2);
    }

    TEST_CASE("entity components survive the round trip")
    {
        const SceneFixture fixture = MakeSceneFixture("SceneComponents");
        const Ref<Scene> source = MakePopulatedScene();

        SceneSerializer writer(source);
        writer.Serialize(fixture.ScenePath);

        const Ref<Scene> loaded = CreateRef<Scene>();
        SceneSerializer reader(loaded);
        REQUIRE(reader.Deserialize(fixture.ScenePath));

        const Entity parent = FindEntityByName(loaded, "Parent");
        REQUIRE(static_cast<bool>(parent));
        CHECK(Near(parent.Transform().Position.x, 1.0f));
        CHECK(loaded->GetChildren(parent).size() == 1);

        const Entity child = FindEntityByName(loaded, "Child");
        REQUIRE(static_cast<bool>(child));
        CHECK(SameUUID(child.GetComponent<RelationshipComponent>().Parent, parent.GetUUID()));
        CHECK(Near(child.Transform().Scale.x, 2.0f));

        const auto& mesh = child.GetComponent<MeshRendererComponent>();
        CHECK(mesh.Primitive == PrimitiveMeshType::Sphere);
        REQUIRE(mesh.Mesh.get() != nullptr);
        CHECK_FALSE(mesh.Mesh->IsEmpty());
        CHECK(Near(mesh.Roughness, 0.25f));
        CHECK(Near(mesh.Metallic, 0.75f));
        CHECK_FALSE(mesh.Visible);

        const auto& rigidbody = child.GetComponent<RigidbodyComponent>();
        CHECK(rigidbody.Type == RigidbodyComponent::RigidbodyType::Kinematic);
        CHECK(Near(rigidbody.LinearVelocity.x, 0.5f));
        CHECK(Near(rigidbody.GravityScale, 0.5f));
        CHECK(rigidbody.IsBullet);

        const auto& collider = child.GetComponent<ColliderComponent>();
        CHECK(collider.ShapeType == ColliderComponent::ColliderShapeType::Capsule);
        CHECK(Near(collider.Radius, 0.4f));
        CHECK(Near(collider.Height, 1.8f));
        CHECK(collider.IsTrigger);
        CHECK(collider.CategoryBits == 0b1010ull);

        const Entity camera = loaded->GetPrimaryCameraEntity();
        REQUIRE(static_cast<bool>(camera));
        CHECK(Near(camera.GetComponent<CameraComponent>().FieldOfView, 65.0f));

        const Entity light = FindEntityByName(loaded, "Sun");
        REQUIRE(static_cast<bool>(light));
        CHECK(light.GetComponent<LightComponent>().Type == LightComponent::LightType::Directional);
        CHECK_FALSE(light.GetComponent<LightComponent>().CastsShadows);

        const auto& scripts = light.GetComponent<ScriptComponent>().Scripts;
        REQUIRE(scripts.size() == 2);
        // The label travels with the reference, so a scene whose script asset is missing still says
        // what it used to point at.
        CHECK(scripts[0].DisplayName == "Rotator.lua");
        CHECK(scripts[0].Enabled);
        CHECK(scripts[1].DisplayName == "Disabled.lua");
        CHECK_FALSE(scripts[1].Enabled);
    }

    TEST_CASE("loading a missing scene file fails instead of throwing")
    {
        const SceneFixture fixture = MakeSceneFixture("SceneMissing");

        const Ref<Scene> scene = CreateRef<Scene>();
        SceneSerializer reader(scene);

        const std::string missingPath =
            (std::filesystem::path(fixture.ScenePath).parent_path() / "Absent.hscene").string();
        CHECK_FALSE(reader.Deserialize(missingPath));
    }

    TEST_CASE("loading a malformed scene file fails instead of throwing")
    {
        const SceneFixture fixture = MakeSceneFixture("SceneMalformed");
        const std::filesystem::path brokenPath = std::filesystem::path(fixture.ScenePath).parent_path() / "Broken.hscene";
        REQUIRE(FileSystem::WriteTextFile(brokenPath, "not: [valid: yaml"));

        const Ref<Scene> scene = CreateRef<Scene>();
        SceneSerializer reader(scene);

        CHECK_FALSE(reader.Deserialize(brokenPath.string()));
    }
}
