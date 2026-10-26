// Box3D's step sub-tasks and the engine's worker pool.
//
// Threading is the one part of the physics layer that can hang instead of fail, so the rules are
// pinned by cases rather than by comments alone: a world configured with a worker count forks its
// step onto the JobSystem and must still simulate the same scene as a single-threaded world, a step
// asked for from a pool worker must be refused rather than parked, and a scheduler that has no pool
// to fork onto must run its tasks inline.
//
// Everything here is headless: scenes are constructed and stepped by hand, no window, no GL context
// and no GPU resource is involved. No case shuts the job system down - Tests/Main.cpp owns it - so
// the "pool is not running" rule is exercised through the pure resolution function instead.

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "Core/JobSystem.h"
#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Math/Math.h"
#include "Physics/PhysicsTaskScheduler.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"

using namespace HachimiEngine;

namespace
{
    constexpr float Step = 1.0f / 60.0f;
    // Parallel and single-threaded runs are compared, not required to be bit-identical: the solver
    // walks the same constraints in a different order, and float addition is not associative.
    constexpr float PositionTolerance = 1e-3f;
    constexpr int BodyCount = 6;
    constexpr int StackSteps = 120;

    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
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

    // A floor with a stack of boxes on it: enough contacts and enough awake bodies for Box3D to have
    // something to fork, and a settled result that is easy to compare between two runs.
    void BuildStack(Scene& scene)
    {
        ClearScene(scene);

        AddBox(scene, "Floor", { 0.0f, 0.0f, 0.0f }, { 6.0f, 0.5f, 6.0f },
               RigidbodyComponent::RigidbodyType::Static);

        for (int index = 0; index < BodyCount; ++index)
        {
            // A small offset per box keeps the stack from being perfectly symmetric, which is what
            // makes the solver do real work instead of resolving one clean vertical overlap.
            const float offset = 0.02f * static_cast<float>(index);
            AddBox(scene,
                   "Box" + std::to_string(index),
                   { offset, 1.5f + 1.05f * static_cast<float>(index), offset },
                   { 0.5f, 0.5f, 0.5f },
                   RigidbodyComponent::RigidbodyType::Dynamic);
        }
    }

    Math::Vec3 PositionOf(Scene& scene, const std::string& name)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            if (entity.GetName() == name)
            {
                return entity.Transform().Position;
            }
        }

        return { 0.0f, 0.0f, 0.0f };
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-3f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    void AdvanceRuntime(Scene& scene, Time& clock, int steps)
    {
        for (int index = 0; index < steps; ++index)
        {
            clock.Advance(Step);
            scene.OnUpdate(Timestep(Step));
        }
    }

    // A running scene, stepped by hand. The runtime is started the way the editor starts it, so the
    // world under test is the one a play session would get.
    void StartRuntime(Scene& scene, Time& clock, uint32_t workerCount)
    {
        scene.GetPhysicsSettings().FixedTimeStep = Step;
        scene.GetPhysicsSettings().WorkerCount = workerCount;
        scene.OnRuntimeStart(clock);
        AdvanceRuntime(scene, clock, 1);
    }

    PhysicsWorld* WorldOf(Scene& scene)
    {
        PhysicsSystem* physics = scene.FindSystem<PhysicsSystem>();
        return physics != nullptr ? physics->GetWorld() : nullptr;
    }
}

TEST_SUITE("Physics")
{
    TEST_CASE("a scene simulates the same with and without forked tasks")
    {
        Scene forkedScene;
        Scene serialScene;
        Time forkedClock;
        Time serialClock;

        BuildStack(forkedScene);
        BuildStack(serialScene);

        // 0 asks for as many workers as the pool has, 1 keeps the whole step on this thread.
        StartRuntime(forkedScene, forkedClock, 0);
        StartRuntime(serialScene, serialClock, 1);

        AdvanceRuntime(forkedScene, forkedClock, StackSteps);
        AdvanceRuntime(serialScene, serialClock, StackSteps);

        for (int index = 0; index < BodyCount; ++index)
        {
            const std::string name = "Box" + std::to_string(index);
            const Math::Vec3 forked = PositionOf(forkedScene, name);
            const Math::Vec3 serial = PositionOf(serialScene, name);

            INFO("box ", index, ": forked (", forked.x, ", ", forked.y, ", ", forked.z, ") vs serial (",
                 serial.x, ", ", serial.y, ", ", serial.z, ")");

            CHECK(Near(forked.x, serial.x, PositionTolerance));
            CHECK(Near(forked.y, serial.y, PositionTolerance));
            CHECK(Near(forked.z, serial.z, PositionTolerance));

            // The stack has to have actually moved, or identical positions mean nothing.
            CHECK(serial.y > 0.4f);
            CHECK(serial.y < 8.0f);
        }
    }

    TEST_CASE("the worker count decides whether the step forks onto the pool")
    {
        const uint32_t poolWorkers = JobSystem::GetWorkerCount();

        Scene forkedScene;
        Scene serialScene;
        Time forkedClock;
        Time serialClock;

        BuildStack(forkedScene);
        BuildStack(serialScene);
        StartRuntime(forkedScene, forkedClock, 0);
        StartRuntime(serialScene, serialClock, 1);

        PhysicsWorld* forked = WorldOf(forkedScene);
        PhysicsWorld* serial = WorldOf(serialScene);
        REQUIRE(forked != nullptr);
        REQUIRE(serial != nullptr);

        AdvanceRuntime(forkedScene, forkedClock, 30);
        AdvanceRuntime(serialScene, serialClock, 30);

        const PhysicsTaskStats forkedStats = forked->GetTaskStats();
        const PhysicsTaskStats serialStats = serial->GetTaskStats();

        INFO("pool workers: ", poolWorkers, ", forked stats: ", forkedStats.WorkerCount, " workers / ",
             forkedStats.PoolTaskCount, " pooled / ", forkedStats.InlineTaskCount, " inline");

        // A single-worker serial world is the behaviour every scene had before threading existed: the
        // tasks Box3D still forks are run by the enqueueing thread, so nothing is missing from the
        // statistics and nothing is waiting on a thread that does not exist.
        CHECK(serialStats.WorkerCount == 1);
        CHECK_FALSE(serialStats.UsesThreadPool);
        CHECK(serialStats.PoolTaskCount == 0);
        CHECK(serialStats.InlineTaskCount > 0);

        if (poolWorkers > 1)
        {
            CHECK(forkedStats.UsesThreadPool);
            CHECK(forkedStats.WorkerCount == poolWorkers);
            CHECK(forkedStats.PoolTaskCount > 0);
            CHECK(forkedStats.InlineTaskCount == 0);
        }
        else
        {
            // A one-core machine leaves nothing to fork onto, so the same fallback applies.
            CHECK(forkedStats.WorkerCount == 1);
            CHECK_FALSE(forkedStats.UsesThreadPool);
        }
    }

    TEST_CASE("a step asked for from a pool worker is refused instead of parking the worker")
    {
        Scene scene;
        Time clock;
        BuildStack(scene);
        StartRuntime(scene, clock, 0);

        PhysicsWorld* world = WorldOf(scene);
        REQUIRE(world != nullptr);

        const Math::Vec3 before = PositionOf(scene, "Box0");

        // What a script or a job that breaks the rule looks like: a step driven from inside pooled
        // work. The scheduler must turn it down - stepping there parks a worker on tasks only the
        // pool can run - and the test only finishes at all because it does.
        std::atomic<bool> ranOnPoolWorker { false };
        JobHandle handle = JobSystem::SubmitTracked([&scene, world, &ranOnPoolWorker]()
        {
            ranOnPoolWorker.store(JobSystem::IsWorkerThread());
            world->Update(scene, Timestep(Step));
        });

        // A refusal that turns into a hang would never return, so the wait is bounded.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!handle.IsComplete() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        REQUIRE(handle.IsComplete());
        CHECK(ranOnPoolWorker.load());
        CHECK(world->GetTaskStats().RefusedStepCount == 1);

        // Refused means not simulated: the scene is untouched rather than half-stepped.
        const Math::Vec3 after = PositionOf(scene, "Box0");
        CHECK(Near(after.x, before.x));
        CHECK(Near(after.y, before.y));
        CHECK(Near(after.z, before.z));
    }

    TEST_CASE("a scheduler without a pool runs its tasks inline and answers already done")
    {
        // WorkerCount = 1 is what a world gets when the pool is not running, so this is the fallback
        // path: Box3D's own serial contract, where the enqueueing callback ran the task itself.
        PhysicsTaskScheduler serial(1);
        CHECK(serial.GetWorkerCount() == 1);
        CHECK_FALSE(serial.UsesThreadPool());

        int runs = 0;
        void* token = serial.Enqueue([](void* context) { *static_cast<int*>(context) += 1; }, &runs);
        CHECK(token == nullptr);
        CHECK(runs == 1);
        serial.Finish(token);

        const PhysicsTaskStats serialStats = serial.GetStats();
        CHECK(serialStats.InlineTaskCount == 1);
        CHECK(serialStats.PoolTaskCount == 0);
        CHECK(serialStats.RefusedStepCount == 0);
    }

    TEST_CASE("a pooled scheduler hands tasks out and waits for them")
    {
        REQUIRE(JobSystem::IsRunning());
        if (JobSystem::GetWorkerCount() <= 1)
        {
            MESSAGE("this machine's pool has a single worker, so there is nothing to hand out");
            return;
        }

        PhysicsTaskScheduler pooled(0);
        CHECK(pooled.UsesThreadPool());
        CHECK(pooled.GetWorkerCount() == JobSystem::GetWorkerCount());

        std::atomic<int> completed { 0 };
        std::vector<void*> tokens;
        for (int index = 0; index < 8; ++index)
        {
            tokens.push_back(pooled.Enqueue([](void* context) { static_cast<std::atomic<int>*>(context)->fetch_add(1); },
                                            &completed));
        }

        // A pooled task cannot be relied on to have run yet; what is pinned here is that every token
        // Finish is given leads to a task that has, which is the guarantee Box3D steps on.
        for (void* token : tokens)
        {
            CHECK(token != nullptr);
            pooled.Finish(token);
        }

        CHECK(completed.load() == 8);
        CHECK(pooled.GetStats().PoolTaskCount == 8);
        CHECK(pooled.GetStats().InlineTaskCount == 0);

        // Slots are recycled, so a second batch reuses them instead of growing the pool of slots.
        std::atomic<int> secondBatch { 0 };
        void* token = pooled.Enqueue([](void* context) { static_cast<std::atomic<int>*>(context)->fetch_add(1); },
                                     &secondBatch);
        pooled.Finish(token);
        CHECK(secondBatch.load() == 1);
    }

    TEST_CASE("a requested worker count resolves against the job system's state")
    {
        using Scheduler = PhysicsTaskScheduler;

        // A single worker is the caller's explicit choice: it means Box3D forks nothing, so it stays
        // 1 even when the pool could offer more.
        CHECK(Scheduler::ResolveWorkerCount(1, true, 8) == 1);
        CHECK(Scheduler::ResolveWorkerCount(4, true, 8) == 4);
        CHECK(Scheduler::ResolveWorkerCount(2, true, 8) == 2);

        // 0 means "as many as the pool has", and never more workers than Box3D accepts.
        CHECK(Scheduler::ResolveWorkerCount(0, true, 8) == 8);
        CHECK(Scheduler::ResolveWorkerCount(0, true, 4096) == Scheduler::MaxWorkers);
        CHECK(Scheduler::ResolveWorkerCount(4096, true, 8) == 8);

        // No pool means no parallelism to give, whatever was asked for - reporting more workers is
        // what would make Box3D fork tasks this scheduler then has to run inline, and some of those
        // spin until the task that orchestrates them starts.
        CHECK(Scheduler::ResolveWorkerCount(0, false, 0) == 1);
        CHECK(Scheduler::ResolveWorkerCount(4, false, 0) == 1);
        CHECK(Scheduler::ResolveWorkerCount(0, true, 0) == 1);
    }
}
