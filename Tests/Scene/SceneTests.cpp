// Scene and Entity without a window: construction, lifetime, duplication and cloning.
//
// This suite exists because Scene used to build a GPU mesh in its constructor, which
// made every test of the ECS, the serializer and the runtime impossible. Geometry is
// CPU-side MeshData now, so the whole scene model is exercised headlessly here. If a
// Scene ever needs an OpenGL context again, this suite fails immediately.
//
// doctest has to be able to print both sides of an assertion, and Entity, UUID and
// Math::Vec3 are deliberately not streamable, so the comparisons below go through
// plain-bool helpers instead of raw CHECK(a == b).

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Renderer/MeshData.h"
#include "Renderer/MeshFactory.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <cmath>

using namespace HachimiEngine;

namespace
{
    bool SameEntity(const Entity& lhs, const Entity& rhs)
    {
        return lhs.GetHandle() == rhs.GetHandle();
    }

    bool IsNull(const Entity& entity)
    {
        return !static_cast<bool>(entity);
    }

    bool SameUUID(UUID lhs, UUID rhs)
    {
        return lhs == rhs;
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    // A Scene starts with a camera, a point light and one cube, so a fresh scene is
    // never empty. Tests that need a clean registry clear it first.
    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
    }

    // entt does not promise an iteration order, so entities are looked up by tag.
    Entity FindEntityByName(Scene& scene, const std::string& name)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            if (entity.HasComponent<TagComponent>() && entity.GetName() == name)
            {
                return entity;
            }
        }
        return Entity {};
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("the default scene is built without an OpenGL context")
    {
        Scene scene;

        CHECK(scene.GetAllEntities().size() == 3);
        CHECK_FALSE(IsNull(scene.GetPrimaryCameraEntity()));
        CHECK_FALSE(IsNull(FindEntityByName(scene, "Camera")));
        CHECK_FALSE(IsNull(FindEntityByName(scene, "Point Light")));

        const Entity cube = FindEntityByName(scene, "Cube");
        REQUIRE_FALSE(IsNull(cube));
        REQUIRE(cube.HasComponent<MeshComponent>());

        const Ref<MeshData>& mesh = cube.GetComponent<MeshComponent>().Mesh;
        REQUIRE(mesh.get() != nullptr);
        CHECK_FALSE(mesh->IsEmpty());
        CHECK(mesh->GetBounds().IsValid());
    }

    TEST_CASE("created entities get an ID, a tag and a transform")
    {
        Scene scene;
        ClearScene(scene);

        const Entity entity = scene.CreateEntity("Player");

        CHECK(entity.GetName() == "Player");
        CHECK_FALSE(SameUUID(entity.GetUUID(), UUID::Invalid()));
        CHECK(entity.HasComponent<TransformComponent>());
        CHECK(entity.HasComponent<RelationshipComponent>());
        CHECK(SameEntity(scene.GetEntityByUUID(entity.GetUUID()), entity));
        CHECK(IsNull(scene.GetEntityByUUID(UUID::Invalid())));
    }

    TEST_CASE("an unnamed entity falls back to the default tag")
    {
        Scene scene;
        CHECK(scene.CreateEntity("").GetName() == "Entity");
    }

    TEST_CASE("destroying an entity removes it from the registry and the UUID map")
    {
        Scene scene;
        ClearScene(scene);

        const Entity entity = scene.CreateEntity("Temp");
        const UUID id = entity.GetUUID();

        scene.DestroyEntity(entity);

        CHECK(scene.GetAllEntities().empty());
        CHECK(IsNull(scene.GetEntityByUUID(id)));
    }

    TEST_CASE("destroying an invalid entity is ignored")
    {
        Scene scene;
        const size_t before = scene.GetAllEntities().size();

        scene.DestroyEntity(Entity {});

        CHECK(scene.GetAllEntities().size() == before);
    }

    TEST_CASE("destroying a parent destroys its children")
    {
        Scene scene;
        ClearScene(scene);

        Entity parent = scene.CreateEntity("Parent");
        Entity child = scene.CreateEntity("Child");
        child.GetComponent<RelationshipComponent>().Parent = parent.GetUUID();
        parent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());

        scene.DestroyEntity(parent);

        CHECK(scene.GetAllEntities().empty());
    }

    TEST_CASE("duplicating an entity copies every component it carries")
    {
        Scene scene;
        ClearScene(scene);

        Entity source = scene.CreateEntity("Source");
        source.Transform().Position = { 1.0f, 2.0f, 3.0f };
        source.AddComponent<MeshComponent>().Mesh = MeshFactory::CreateSphere();
        source.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;
        source.AddComponent<ColliderComponent>().Radius = 0.25f;
        source.AddComponent<CameraComponent>().FieldOfView = 70.0f;
        source.AddComponent<LightComponent>().Intensity = 3.0f;
        source.AddComponent<ScriptComponent>().Scripts.emplace_back();

        const Entity duplicate = scene.DuplicateEntity(source);

        REQUIRE_FALSE(IsNull(duplicate));
        CHECK(duplicate.GetName() == "Source Copy");
        CHECK_FALSE(SameUUID(duplicate.GetUUID(), source.GetUUID()));

        const TransformComponent& sourceTransform = source.GetComponent<TransformComponent>();
        const TransformComponent& duplicateTransform = duplicate.GetComponent<TransformComponent>();
        CHECK(Near(duplicateTransform.Position.x, sourceTransform.Position.x));
        CHECK(Near(duplicateTransform.Position.y, sourceTransform.Position.y));
        CHECK(Near(duplicateTransform.Position.z, sourceTransform.Position.z));

        // Geometry is shared; only the material override is cloned.
        CHECK(duplicate.GetComponent<MeshComponent>().Mesh.get() == source.GetComponent<MeshComponent>().Mesh.get());
        CHECK(duplicate.GetComponent<RigidbodyComponent>().Type == RigidbodyComponent::RigidbodyType::Static);
        CHECK(Near(duplicate.GetComponent<ColliderComponent>().Radius, 0.25f));
        CHECK(Near(duplicate.GetComponent<CameraComponent>().FieldOfView, 70.0f));
        CHECK(Near(duplicate.GetComponent<LightComponent>().Intensity, 3.0f));
        CHECK(duplicate.GetComponent<ScriptComponent>().Scripts.size() == 1);
        CHECK(scene.GetAllEntities().size() == 2);
    }

    TEST_CASE("cloning a scene keeps UUIDs, hierarchy and shared geometry")
    {
        Scene scene;
        ClearScene(scene);
        scene.SetName("Original");
        scene.GetEnvironmentSettings().Exposure = 1.75f;
        scene.GetPhysicsSettings().Gravity = { 0.0f, -20.0f, 0.0f };

        Entity parent = scene.CreateEntity("Parent");
        Entity child = scene.CreateEntity("Child");
        child.GetComponent<RelationshipComponent>().Parent = parent.GetUUID();
        parent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());
        child.Transform().Position = { 0.0f, 5.0f, 0.0f };
        child.AddComponent<MeshComponent>().Mesh = MeshFactory::CreateCube();

        const Ref<Scene> clone = scene.Clone();

        REQUIRE(clone.get() != nullptr);
        CHECK(clone->GetName() == "Original");
        CHECK(clone->GetAllEntities().size() == 2);
        CHECK(Near(clone->GetEnvironmentSettings().Exposure, 1.75f));
        CHECK(Near(clone->GetPhysicsSettings().Gravity.y, -20.0f));

        const Entity clonedChild = clone->GetEntityByUUID(child.GetUUID());
        REQUIRE_FALSE(IsNull(clonedChild));
        CHECK(clonedChild.GetName() == "Child");
        CHECK_FALSE(IsNull(clone->GetEntityByUUID(parent.GetUUID())));

        // The GPU mesh is uploaded per scene, but the CPU geometry stays shared.
        CHECK(clonedChild.GetComponent<MeshComponent>().Mesh.get() == child.GetComponent<MeshComponent>().Mesh.get());
        CHECK(Near(clone->GetWorldTransform(clonedChild.GetHandle())[3].y, 5.0f));

        // The clone is independent: destroying one side leaves the other intact.
        clone->DestroyEntity(clonedChild);
        CHECK_FALSE(IsNull(scene.GetEntityByUUID(child.GetUUID())));
    }

    TEST_CASE("world transforms compose through the parent chain")
    {
        Scene scene;
        ClearScene(scene);

        Entity parent = scene.CreateEntity("Parent");
        parent.Transform().Position = { 10.0f, 0.0f, 0.0f };

        Entity child = scene.CreateEntity("Child");
        child.Transform().Position = { 0.0f, 2.0f, 0.0f };
        child.GetComponent<RelationshipComponent>().Parent = parent.GetUUID();

        const Math::Mat4 world = scene.GetWorldTransform(child.GetHandle());
        CHECK(Near(world[3].x, 10.0f));
        CHECK(Near(world[3].y, 2.0f));
    }

    TEST_CASE("runtime start and stop stay headless")
    {
        Scene scene;
        ClearScene(scene);

        Entity ground = scene.CreateEntity("Ground");
        ground.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;
        ground.AddComponent<ColliderComponent>();

        scene.OnRuntimeStart();
        CHECK(scene.IsPhysicsRunning());

        scene.OnUpdate(Timestep(1.0f / 60.0f));

        scene.OnRuntimeStop();
        CHECK_FALSE(scene.IsPhysicsRunning());
    }
}
