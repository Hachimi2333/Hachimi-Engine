// Physics queries and rigid body motion state.
//
// The engine could create and simulate bodies but not ask the world anything: picking, line of
// sight, area triggers and every "what is under this point" gameplay feature had to be written
// against the ECS instead. These cases drive the query and motion-state API of PhysicsWorld
// directly, on a scene whose runtime was started by hand, so no window and no GL context is
// involved - Box3D is a CPU library and a scene runs headless.
//
// The entity keys the queries report are resolved through Scene::GetEntityByKey, which is the same
// path the editor's picking and the script bindings take.

#include <doctest/doctest.h>

#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Physics/PhysicsQuery.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Math/Math.h"

#include <cmath>
#include <string>

using namespace HachimiEngine;

namespace
{
    constexpr float Step = 1.0f / 60.0f;

    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
    }

    // A scene whose physics world exists and holds the bodies that are already in the scene.
    //
    // The runtime is started with the caller's clock and the physics system is stepped twice: the
    // first step creates bodies for the static entities, the second gives the dynamic ones a
    // simulated state to report. Doing this by hand is what makes the whole suite headless.
    void StartPhysics(Scene& scene, Time& clock)
    {
        scene.OnRuntimeStart(clock);
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));
    }

    void StopPhysics(Scene& scene)
    {
        scene.OnRuntimeStop();
    }

    PhysicsWorld* WorldOf(Scene& scene)
    {
        PhysicsSystem* physics = scene.FindSystem<PhysicsSystem>();
        return physics != nullptr ? physics->GetWorld() : nullptr;
    }

    Entity AddBox(Scene& scene,
                  const std::string& name,
                  const Math::Vec3& position,
                  const Math::Vec3& halfExtents,
                  RigidbodyComponent::RigidbodyType type)
    {
        Entity entity = scene.CreateEntity(name);
        entity.Transform().Position = position;

        entity.AddComponent<RigidbodyComponent>().Type = type;

        auto& collider = entity.AddComponent<ColliderComponent>();
        collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
        collider.HalfExtents = halfExtents;
        return entity;
    }

    uint64_t KeyOf(const Entity& entity)
    {
        return static_cast<uint64_t>(entity.GetHandle());
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-3f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool Near(const Math::Vec3& lhs, const Math::Vec3& rhs, float tolerance = 1e-3f)
    {
        return Near(lhs.x, rhs.x, tolerance) && Near(lhs.y, rhs.y, tolerance) && Near(lhs.z, rhs.z, tolerance);
    }

    // A box under a ray cast straight down, sitting on the y = 0 plane with its top at y = 1.
    struct RayScene
    {
        Scene SceneUnderTest;
        Time Clock;

        RayScene()
        {
            ClearScene(SceneUnderTest);
            Box = AddBox(
                SceneUnderTest,
                "Ceiling",
                { 0.0f, 0.5f, 0.0f },
                { 1.0f, 0.5f, 1.0f },
                RigidbodyComponent::RigidbodyType::Static);
            StartPhysics(SceneUnderTest, Clock);
        }

        ~RayScene()
        {
            StopPhysics(SceneUnderTest);
        }

        Entity Box;
    };
}

TEST_SUITE("Physics")
{
    TEST_CASE("a ray cast reports the hit point, the normal and the distance")
    {
        RayScene world;
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        REQUIRE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hit));

        CHECK(hit.EntityKey == KeyOf(world.Box));
        // The top face of the box is at y = 1, so the ray travelled four units to reach it.
        CHECK(Near(hit.Distance, 4.0f));
        CHECK(Near(hit.Point, Math::Vec3(0.0f, 1.0f, 0.0f)));
        CHECK(Near(hit.Normal, Math::Vec3(0.0f, 1.0f, 0.0f)));
    }

    TEST_CASE("a ray cast that misses reports nothing and leaves the result alone")
    {
        RayScene world;
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        hit.EntityKey = 12345;

        // Aimed away from the box, and aimed at it from beyond its distance: both are misses.
        CHECK_FALSE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 10.0f, hit));
        CHECK_FALSE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 1.0f, hit));
        CHECK(hit.EntityKey == 12345);

        // A zero-length ray is not a hit either, and must not reach Box3D as a degenerate query.
        CHECK_FALSE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 10.0f, hit));
        CHECK_FALSE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 0.0f, hit));
    }

    TEST_CASE("the ray is measured in world units, not in fractions of the direction vector")
    {
        RayScene world;
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        // A direction that is not a unit vector must not change what "four units away" means.
        REQUIRE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -37.0f, 0.0f }, 10.0f, hit));
        CHECK(Near(hit.Distance, 4.0f));
    }

    TEST_CASE("every shape along a ray is reported with its own distance")
    {
        Scene scene;
        ClearScene(scene);

        Entity upper = AddBox(scene, "Upper", { 0.0f, 2.0f, 0.0f }, { 1.0f, 0.5f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);
        Entity lower = AddBox(scene, "Lower", { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.5f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        RayCastResults hits;
        const size_t count = physics->RayCastAll({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hits);

        CHECK(count == 2);
        CHECK(hits.size() == 2);

        // The closest hit is the same one the single-hit query returns.
        RayCastResult closest;
        REQUIRE(physics->RayCast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, closest));
        CHECK(closest.EntityKey == KeyOf(upper));
        CHECK(Near(closest.Distance, 2.5f));

        bool sawUpper = false;
        bool sawLower = false;
        for (const RayCastResult& hit : hits)
        {
            if (hit.EntityKey == KeyOf(upper))
            {
                sawUpper = true;
                CHECK(Near(hit.Distance, 2.5f));
            }
            else if (hit.EntityKey == KeyOf(lower))
            {
                sawLower = true;
                CHECK(Near(hit.Distance, 4.5f));
            }
        }

        CHECK(sawUpper);
        CHECK(sawLower);

        // Appending rather than replacing is what the signature promises.
        RayCastResults appended;
        appended.push_back(RayCastResult {});
        CHECK(physics->RayCastAll({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, appended) == 2);
        CHECK(appended.size() == 3);

        // The result cap is respected rather than ignored.
        RayCastResults capped;
        CHECK(physics->RayCastAll({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, capped, 1) == 1);
        CHECK(capped.size() == 1);

        StopPhysics(scene);
    }

    TEST_CASE("a miss and a zero cap append nothing")
    {
        RayScene world;
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);

        RayCastResults hits;
        CHECK(physics->RayCastAll({ 0.0f, 5.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 10.0f, hits) == 0);
        CHECK(hits.empty());

        CHECK(physics->RayCastAll({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hits, 0) == 0);
        CHECK(hits.empty());
    }

    TEST_CASE("an overlap sphere finds what its volume reaches and nothing beyond it")
    {
        Scene scene;
        ClearScene(scene);

        Entity near = AddBox(scene, "Near", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Static);
        Entity far = AddBox(scene, "Far", { 10.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        OverlapResults results;
        CHECK(physics->OverlapSphere({ 0.0f, 0.0f, 0.0f }, 2.0f, results) == 1);
        REQUIRE(results.size() == 1);
        CHECK(results[0].EntityKey == KeyOf(near));
        CHECK(Near(results[0].Position, Math::Vec3(0.0f, 0.0f, 0.0f)));

        // The same query at the far box's position must find that one instead.
        OverlapResults farResults;
        CHECK(physics->OverlapSphere({ 10.0f, 0.0f, 0.0f }, 1.0f, farResults) == 1);
        REQUIRE(farResults.size() == 1);
        CHECK(farResults[0].EntityKey == KeyOf(far));

        // A sphere that reaches neither.
        OverlapResults empty;
        CHECK(physics->OverlapSphere({ 5.0f, 0.0f, 0.0f }, 1.0f, empty) == 0);
        CHECK(empty.empty());

        // A degenerate radius is refused rather than passed on.
        CHECK(physics->OverlapSphere({ 0.0f, 0.0f, 0.0f }, 0.0f, empty) == 0);

        StopPhysics(scene);
    }

    TEST_CASE("an overlap box uses its rotation instead of a loose axis-aligned bound")
    {
        Scene scene;
        ClearScene(scene);

        // A plate that is wide along X and Z and thin along Y. Quarter turns around Z make it tall
        // and thin instead, which is what separates a real rotated overlap from a loose
        // axis-aligned bound: the same query must answer opposite ways in the two orientations.
        Entity plate = AddBox(scene, "Plate", { 0.0f, 0.0f, 0.0f }, { 2.0f, 0.05f, 2.0f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        const Math::Vec3 probeHalfExtents(0.25f, 0.25f, 0.25f);
        const Math::Vec3 beside { 1.5f, 0.0f, 0.0f };
        const Math::Vec3 above { 0.0f, 1.5f, 0.0f };
        const Math::Quat noRotation(1.0f, 0.0f, 0.0f, 0.0f);
        const Math::Quat onEdge(Math::Radians(Math::Vec3(0.0f, 0.0f, 90.0f)));

        // Flat: a probe beside it is inside the slab, a probe above it is in the air.
        OverlapResults hits;
        CHECK(physics->OverlapBox(beside, probeHalfExtents, noRotation, hits) == 1);
        REQUIRE(hits.size() == 1);
        CHECK(hits[0].EntityKey == KeyOf(plate));

        hits.clear();
        CHECK(physics->OverlapBox(above, probeHalfExtents, noRotation, hits) == 0);

        // On edge: the two answers swap, because the shape moved and not the query.
        plate.Transform().Rotation = { 0.0f, 0.0f, 90.0f };
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));

        OverlapResults after;
        CHECK(physics->OverlapBox(beside, probeHalfExtents, noRotation, after) == 0);
        CHECK(physics->OverlapBox(above, probeHalfExtents, noRotation, after) == 1);
        REQUIRE(after.size() == 1);
        CHECK(after[0].EntityKey == KeyOf(plate));

        // The query volume's own rotation is applied as well: an elongated probe reaches the
        // standing plate sideways when it is laid across, and misses when it stands along Y.
        plate.Transform().Rotation = { 0.0f, 0.0f, 0.0f };
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));

        const Math::Vec3 acrossHalfExtents(1.5f, 0.1f, 0.1f);
        OverlapResults probe;
        CHECK(physics->OverlapBox({ 3.0f, 0.0f, 0.0f }, acrossHalfExtents, noRotation, probe) == 1);

        probe.clear();
        CHECK(physics->OverlapBox(
                  { 3.0f, 0.0f, 0.0f },
                  acrossHalfExtents,
                  Math::Quat(Math::Radians(Math::Vec3(0.0f, 0.0f, 90.0f))),
                  probe)
            == 0);

        StopPhysics(scene);
    }

    TEST_CASE("a swept sphere stops at the wall in front of it")
    {
        Scene scene;
        ClearScene(scene);

        Entity wall = AddBox(scene, "Wall", { 5.0f, 0.0f, 0.0f }, { 0.5f, 4.0f, 4.0f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        ShapeCastResult hit;
        REQUIRE(physics->ShapeCastSphere({ 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, 0.5f, hit));

        CHECK(hit.EntityKey == KeyOf(wall));
        // The wall's near face is at x = 4.5; a radius 0.5 sphere stops 0.5 short of it.
        CHECK(Near(hit.Distance, 4.0f, 0.05f));
        CHECK(Near(hit.Normal, Math::Vec3(-1.0f, 0.0f, 0.0f), 0.05f));

        // A sweep that stops short of the wall finds nothing.
        ShapeCastResult missing;
        CHECK_FALSE(physics->ShapeCastSphere({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 0.5f, missing));

        StopPhysics(scene);
    }

    TEST_CASE("the query filter's bits decide which shapes a query may hit")
    {
        Scene scene;
        ClearScene(scene);

        constexpr uint64_t GroundCategory = 1ull << 0;
        constexpr uint64_t EnemyCategory = 1ull << 1;

        Entity ground = AddBox(scene, "Ground", { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.5f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);
        ground.GetComponent<ColliderComponent>().CategoryBits = GroundCategory;

        Entity enemy = AddBox(scene, "Enemy", { 3.0f, 0.0f, 0.0f }, { 1.0f, 0.5f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);
        enemy.GetComponent<ColliderComponent>().CategoryBits = EnemyCategory;

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        // Hits the ground and the enemy when the mask accepts both categories.
        const Math::Vec3 origin { -5.0f, 0.0f, 0.0f };
        const Math::Vec3 direction { 1.0f, 0.0f, 0.0f };

        RayCastResult hit;
        REQUIRE(physics->RayCast(origin, direction, 20.0f, hit));
        CHECK(hit.EntityKey == KeyOf(ground));

        // A mask that only accepts the enemy category passes through the ground.
        PhysicsQueryFilter enemyOnly;
        enemyOnly.MaskBits = EnemyCategory;
        REQUIRE(physics->RayCast(origin, direction, 20.0f, hit, enemyOnly));
        CHECK(hit.EntityKey == KeyOf(enemy));

        // A mask that accepts nothing is refused instead of answered with an empty result.
        PhysicsQueryFilter nothing;
        nothing.MaskBits = 0;
        RayCastResult untouched;
        untouched.EntityKey = 7;
        CHECK_FALSE(physics->RayCast(origin, direction, 20.0f, untouched, nothing));
        CHECK(untouched.EntityKey == 7);

        // Excluding the entity that fired the ray is how a self-query avoids its own collider.
        PhysicsQueryFilter notGround;
        notGround.ExcludeEntityKey = KeyOf(ground);
        REQUIRE(physics->RayCast(origin, direction, 20.0f, hit, notGround));
        CHECK(hit.EntityKey == KeyOf(enemy));

        // The exclusion is by identity, and the mask is by category, so the two compose: a query
        // that may only hit the ground but must not hit this ground finds nothing.
        PhysicsQueryFilter groundButNotThisOne;
        groundButNotThisOne.MaskBits = GroundCategory;
        groundButNotThisOne.ExcludeEntityKey = KeyOf(ground);
        CHECK_FALSE(physics->RayCast(origin, direction, 20.0f, hit, groundButNotThisOne));

        // The same exclusion applies to the collecting and overlap queries.
        RayCastResults collected;
        CHECK(physics->RayCastAll(origin, direction, 20.0f, collected, DefaultMaxQueryResults, notGround) == 1);
        REQUIRE(collected.size() == 1);
        CHECK(collected[0].EntityKey == KeyOf(enemy));

        OverlapResults overlaps;
        PhysicsQueryFilter notGroundOverlap;
        notGroundOverlap.ExcludeEntityKey = KeyOf(ground);
        CHECK(physics->OverlapSphere({ 0.0f, 0.0f, 0.0f }, 1.0f, overlaps, DefaultMaxQueryResults, notGroundOverlap) == 0);

        StopPhysics(scene);
    }

    TEST_CASE("a destroyed entity stops being reported by queries")
    {
        Scene scene;
        ClearScene(scene);

        Entity doomed = AddBox(scene, "Doomed", { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);
        Entity survivor = AddBox(scene, "Survivor", { 0.0f, 5.0f, 0.0f }, { 1.0f, 0.5f, 1.0f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        REQUIRE(WorldOf(scene) != nullptr);

        scene.DestroyEntity(doomed);

        // Destroying the entity clears the body's owner, so a query that still reaches the shape
        // must not name an entity that no longer exists.
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));

        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        REQUIRE(physics->RayCast({ 0.0f, 20.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 40.0f, hit));
        CHECK(hit.EntityKey == KeyOf(survivor));
        CHECK_FALSE(scene.GetEntityByKey(hit.EntityKey) == Entity {});

        // The destroyed entity's key resolves to nothing even if a stale one is handed back.
        CHECK_FALSE(static_cast<bool>(scene.GetEntityByKey(KeyOf(doomed))));

        StopPhysics(scene);
    }

    TEST_CASE("queries on a world with no bodies answer empty instead of crashing")
    {
        Scene scene;
        ClearScene(scene);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        CHECK_FALSE(physics->RayCast({ 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, hit));

        RayCastResults rays;
        CHECK(physics->RayCastAll({ 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, rays) == 0);

        OverlapResults overlaps;
        CHECK(physics->OverlapSphere({ 0.0f, 0.0f, 0.0f }, 100.0f, overlaps) == 0);

        ShapeCastResult cast;
        CHECK_FALSE(physics->ShapeCastSphere({ 0.0f, 0.0f, 0.0f }, { 0.0f, -100.0f, 0.0f }, 1.0f, cast));

        StopPhysics(scene);
    }

    TEST_CASE("an entity without a body reports the neutral motion state")
    {
        Scene scene;
        ClearScene(scene);

        Entity bodiless = scene.CreateEntity("No Body");
        Entity box = AddBox(scene, "Box", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Dynamic);

        Time clock;
        StartPhysics(scene, clock);
        PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        const uint64_t unknown = KeyOf(bodiless) + 100000;
        CHECK_FALSE(physics->HasBody(unknown));

        CHECK(Near(physics->GetLinearVelocity(unknown), Math::Vec3(0.0f)));
        CHECK(Near(physics->GetAngularVelocity(unknown), Math::Vec3(0.0f)));
        CHECK(Near(physics->GetMass(unknown), 0.0f));
        CHECK_FALSE(physics->IsAwake(unknown));

        // Every setter is a no-op rather than a crash.
        physics->SetLinearVelocity(unknown, { 1.0f, 0.0f, 0.0f });
        physics->SetAngularVelocity(unknown, { 1.0f, 0.0f, 0.0f });
        physics->ApplyForce(unknown, { 0.0f, 1.0f, 0.0f });
        physics->ApplyImpulse(unknown, { 0.0f, 1.0f, 0.0f });
        physics->ApplyTorque(unknown, { 0.0f, 1.0f, 0.0f });
        physics->ApplyAngularImpulse(unknown, { 0.0f, 1.0f, 0.0f });
        physics->SetAwake(unknown, true);
        physics->Teleport(unknown, { 1.0f, 1.0f, 1.0f }, Math::Quat(1.0f, 0.0f, 0.0f, 0.0f), scene);

        // The dynamic box does have one, and its collider gives it a mass.
        CHECK(physics->HasBody(KeyOf(box)));
        CHECK(physics->GetMass(KeyOf(box)) > 0.0f);

        StopPhysics(scene);
    }

    TEST_CASE("an impulse changes the velocity the world reports")
    {
        Scene scene;
        ClearScene(scene);

        Entity box = AddBox(scene, "Box", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Dynamic);
        // Gravity would otherwise hide the impulse behind a downward acceleration.
        box.GetComponent<RigidbodyComponent>().GravityScale = 0.0f;

        Time clock;
        StartPhysics(scene, clock);
        PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        const uint64_t key = KeyOf(box);
        const float mass = physics->GetMass(key);
        REQUIRE(mass > 0.0f);

        // An impulse of mass * 2 along X is a velocity change of 2 m/s.
        physics->ApplyImpulse(key, { mass * 2.0f, 0.0f, 0.0f });
        CHECK(Near(physics->GetLinearVelocity(key).x, 2.0f, 0.05f));

        // Setting the velocity directly is the same channel.
        physics->SetLinearVelocity(key, { 0.0f, 0.0f, 3.0f });
        CHECK(Near(physics->GetLinearVelocity(key), Math::Vec3(0.0f, 0.0f, 3.0f), 0.05f));

        physics->SetAngularVelocity(key, { 0.0f, 1.5f, 0.0f });
        CHECK(Near(physics->GetAngularVelocity(key).y, 1.5f, 0.05f));

        physics->ApplyAngularImpulse(key, { 0.0f, 0.0f, 0.0f });
        physics->ApplyTorque(key, { 0.0f, 0.0f, 0.0f });
        physics->ApplyForce(key, { 0.0f, 0.0f, 0.0f });
        physics->ApplyForceAtPoint(key, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        physics->ApplyImpulseAtPoint(key, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

        // Sleep state is observable and settable.
        physics->SetAwake(key, false);
        CHECK_FALSE(physics->IsAwake(key));
        physics->SetAwake(key, true);
        CHECK(physics->IsAwake(key));

        StopPhysics(scene);
    }

    TEST_CASE("a teleport moves the body, the scene transform and the interpolation history")
    {
        Scene scene;
        ClearScene(scene);

        Entity box = AddBox(scene, "Box", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Dynamic);
        box.GetComponent<RigidbodyComponent>().GravityScale = 0.0f;

        Time clock;
        StartPhysics(scene, clock);
        PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        const uint64_t key = KeyOf(box);
        const Math::Quat rotation = Math::Quat(Math::Radians(Math::Vec3(0.0f, 90.0f, 0.0f)));
        physics->Teleport(key, { 4.0f, 3.0f, 2.0f }, rotation, scene);

        // The authored transform follows immediately, so the entity is where it was asked to be
        // even before the next step.
        CHECK(Near(box.Transform().Position, Math::Vec3(4.0f, 3.0f, 2.0f)));
        CHECK(Near(box.Transform().Rotation.y, 90.0f, 0.05f));

        // The interpolation history was rewritten too, so the next frame does not draw the entity
        // sliding in from its old position.
        const auto& rigidbody = box.GetComponent<RigidbodyComponent>();
        CHECK(Near(rigidbody.PreviousPosition, Math::Vec3(4.0f, 3.0f, 2.0f)));
        CHECK(Near(rigidbody.PreviousRotation.y, 90.0f, 0.05f));

        // And the body itself moved: a ray down its column hits it at its new home.
        RayCastResult hit;
        REQUIRE(physics->RayCast({ 4.0f, 20.0f, 2.0f }, { 0.0f, -1.0f, 0.0f }, 40.0f, hit));
        CHECK(hit.EntityKey == key);

        // A simulated step then keeps it there rather than snapping it back.
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));
        CHECK(Near(box.Transform().Position, Math::Vec3(4.0f, 3.0f, 2.0f), 0.05f));

        StopPhysics(scene);
    }

    TEST_CASE("a teleport of a parented body composes the parent chain once")
    {
        Scene scene;
        ClearScene(scene);

        Entity parent = scene.CreateEntity("Parent");
        parent.Transform().Position = { 10.0f, 0.0f, 0.0f };

        Entity child = AddBox(scene, "Child", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Dynamic);
        child.GetComponent<RigidbodyComponent>().GravityScale = 0.0f;
        scene.SetParent(child, parent);

        Time clock;
        StartPhysics(scene, clock);
        PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        physics->Teleport(KeyOf(child), { 10.0f, 2.0f, 0.0f }, Math::Quat(1.0f, 0.0f, 0.0f, 0.0f), scene);

        // The request was a world position, so the stored local position is the parent-relative one.
        CHECK(Near(child.Transform().Position, Math::Vec3(0.0f, 2.0f, 0.0f)));
        CHECK(Near(scene.GetWorldTransform(child.GetHandle())[3].y, 2.0f));
        CHECK(Near(scene.GetWorldTransform(child.GetHandle())[3].x, 10.0f));

        StopPhysics(scene);
    }

    TEST_CASE("a physics query key resolves back to the entity that owns the shape")
    {
        Scene scene;
        ClearScene(scene);

        Entity box = AddBox(scene, "Box", { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Static);

        Time clock;
        StartPhysics(scene, clock);
        const PhysicsWorld* physics = WorldOf(scene);
        REQUIRE(physics != nullptr);

        RayCastResult hit;
        REQUIRE(physics->RayCast({ 0.0f, 10.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 20.0f, hit));

        const Entity resolved = scene.GetEntityByKey(hit.EntityKey);
        REQUIRE(static_cast<bool>(resolved));
        CHECK(resolved.GetUUID() == box.GetUUID());
        CHECK(resolved.GetName() == "Box");

        // A key that names nothing resolves to an empty entity rather than a stale handle.
        CHECK_FALSE(static_cast<bool>(scene.GetEntityByKey(0)));
        CHECK_FALSE(static_cast<bool>(scene.GetEntityByKey(0x7fffffff)));

        StopPhysics(scene);
    }
}

TEST_SUITE("Physics")
{
    TEST_CASE("registry handle zero participates in queries and physics writeback")
    {
        Scene scene;
        Entity first = scene.GetEntityByKey(0);
        REQUIRE(first);
        first.Transform().Position = { 100, 8, 0 };
        first.AddComponent<RigidbodyComponent>();
        first.AddComponent<ColliderComponent>();
        PhysicsWorld world(scene.GetPhysicsSettings());
        world.CreateBodies(scene);
        RayCastResult hit;
        REQUIRE(world.RayCast({ 100, 12, 0 }, { 0, -1, 0 }, 10, hit));
        CHECK(hit.EntityKey == 0);
        PhysicsQueryFilter filter;
        filter.ExcludeEntityKey = 0;
        CHECK_FALSE(world.RayCast({ 100, 12, 0 }, { 0, -1, 0 }, 10, hit, filter));
        world.Update(scene, Timestep(1.0f / 60.0f));
        CHECK(first.Transform().Position.y < 8.0f);
    }
}

TEST_SUITE("Physics")
{
    TEST_CASE("RayCastAll caps nearest hits and appends independently of existing results")
    {
        Scene scene;
        ClearScene(scene);
        const auto near = AddBox(scene, "Near", { 0, 4, 0 }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Static);
        AddBox(scene, "Far", { 0, 0, 0 }, { 0.5f, 0.5f, 0.5f }, RigidbodyComponent::RigidbodyType::Static);
        PhysicsWorld world(scene.GetPhysicsSettings());
        world.CreateBodies(scene);
        RayCastResults hits(1);
        CHECK(world.RayCastAll({ 0, 10, 0 }, { 0, -1, 0 }, 20, hits, 1) == 1);
        REQUIRE(hits.size() == 2);
        CHECK(hits[1].EntityKey == static_cast<uint64_t>(near.GetHandle()));
        RayCastResults all;
        CHECK(world.RayCastAll({ 0, 10, 0 }, { 0, -1, 0 }, 20, all) == 2);
        REQUIRE(all.size() == 2);
        CHECK(all[0].Distance < all[1].Distance);
        OverlapResults overlaps(1);
        CHECK(world.OverlapSphere({ 0, 2, 0 }, 5, overlaps, 1) == 1);
        CHECK(overlaps.size() == 2);
    }
}
