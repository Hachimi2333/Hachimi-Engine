// SceneSystem: the phase scheduler, and the physics/scripting decoupling it buys.
//
// Scene::OnUpdate used to hard-code "physics, then scripts" and return early when the physics
// world was missing, so the two could not be exercised apart and a Box3D failure silently
// disabled every script. These cases pin the ordering rules and the independence.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Scene/Scene.h"
#include "Scene/SceneSystem.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Scene/Systems/ScriptSystem.h"

#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    // Records every OnUpdate it receives, tagged with its own name and phase.
    class RecordingSystem final : public SceneSystem
    {
    public:
        RecordingSystem(std::string name, ScenePhase phase, std::vector<std::string>& log, int& attachCount, int& detachCount)
            : m_Name(std::move(name))
            , m_Phase(phase)
            , m_Log(log)
            , m_AttachCount(attachCount)
            , m_DetachCount(detachCount)
        {
        }

        std::string_view GetName() const override { return m_Name; }
        ScenePhase GetPhase() const override { return m_Phase; }

        void OnAttach(Scene& scene) override
        {
            (void)scene;
            ++m_AttachCount;
        }

        void OnUpdate(Scene& scene, Timestep timestep) override
        {
            (void)scene;
            (void)timestep;
            m_Log.push_back(m_Name);
        }

        void OnDetach(Scene& scene) override
        {
            (void)scene;
            ++m_DetachCount;
        }

    private:
        std::string m_Name;
        ScenePhase m_Phase;
        std::vector<std::string>& m_Log;
        int& m_AttachCount;
        int& m_DetachCount;
    };
}

TEST_SUITE("Scene")
{
    TEST_CASE("systems run in phase order, then in the order they were added")
    {
        Scene scene;
        std::vector<std::string> log;
        int attachCount = 0;
        int detachCount = 0;

        // Added out of phase order on purpose: the scene has to reorder them.
        scene.AddSystem<RecordingSystem>("late", ScenePhase::LateUpdate, log, attachCount, detachCount);
        scene.AddSystem<RecordingSystem>("pre-a", ScenePhase::PreUpdate, log, attachCount, detachCount);
        scene.AddSystem<RecordingSystem>("fixed", ScenePhase::FixedUpdate, log, attachCount, detachCount);
        scene.AddSystem<RecordingSystem>("pre-b", ScenePhase::PreUpdate, log, attachCount, detachCount);
        scene.AddSystem<RecordingSystem>("update", ScenePhase::Update, log, attachCount, detachCount);

        CHECK(attachCount == 5);

        scene.OnUpdate(Timestep(1.0f / 60.0f));

        REQUIRE(log.size() == 5);
        CHECK(log[0] == "pre-a");
        CHECK(log[1] == "pre-b");
        CHECK(log[2] == "fixed");
        CHECK(log[3] == "update");
        CHECK(log[4] == "late");
    }

    TEST_CASE("a removed system stops running and is detached once")
    {
        Scene scene;
        std::vector<std::string> log;
        int attachCount = 0;
        int detachCount = 0;

        RecordingSystem& keeper = scene.AddSystem<RecordingSystem>("keeper", ScenePhase::Update, log, attachCount, detachCount);
        RecordingSystem& removed = scene.AddSystem<RecordingSystem>("removed", ScenePhase::Update, log, attachCount, detachCount);

        scene.OnUpdate(Timestep(0.0f));
        REQUIRE(log.size() == 2);

        scene.RemoveSystem(removed);
        CHECK(detachCount == 1);

        // Removing it again is a no-op rather than a second detach.
        scene.RemoveSystem(removed);
        CHECK(detachCount == 1);

        log.clear();
        scene.OnUpdate(Timestep(0.0f));
        REQUIRE(log.size() == 1);
        CHECK(log[0] == "keeper");

        CHECK(scene.FindSystem<RecordingSystem>() == &keeper);
    }

    TEST_CASE("a system can be found by its type")
    {
        Scene scene;
        std::vector<std::string> log;
        int attachCount = 0;
        int detachCount = 0;

        CHECK(scene.FindSystem<RecordingSystem>() == nullptr);

        RecordingSystem& system = scene.AddSystem<RecordingSystem>("probe", ScenePhase::Update, log, attachCount, detachCount);
        CHECK(scene.FindSystem<RecordingSystem>() == &system);
        CHECK(system.GetName() == "probe");
    }

    TEST_CASE("starting the runtime attaches physics and scripting, stopping detaches both")
    {
        Scene scene;
        CHECK_FALSE(scene.IsRuntimeRunning());

        scene.OnRuntimeStart();

        CHECK(scene.IsRuntimeRunning());
        CHECK(scene.IsPhysicsRunning());
        CHECK(scene.IsScriptRunning());
        CHECK(scene.FindSystem<PhysicsSystem>() != nullptr);
        CHECK(scene.FindSystem<ScriptSystem>() != nullptr);

        // Updating a running scene must not throw with no entities to simulate.
        scene.OnUpdate(Timestep(1.0f / 60.0f));

        scene.OnRuntimeStop();

        CHECK_FALSE(scene.IsRuntimeRunning());
        CHECK(scene.FindSystem<PhysicsSystem>() == nullptr);
        CHECK(scene.FindSystem<ScriptSystem>() == nullptr);
    }

    TEST_CASE("starting twice is refused rather than doubling the simulation")
    {
        Scene scene;
        scene.OnRuntimeStart();
        scene.OnRuntimeStart();

        scene.OnRuntimeStop();
        scene.OnRuntimeStop();

        CHECK_FALSE(scene.IsRuntimeRunning());
        CHECK_FALSE(scene.IsPhysicsRunning());
    }

    TEST_CASE("scripts run even when physics is not attached")
    {
        Scene scene;

        // Only the scripting system: the scene must still update it rather than bailing out.
        ScriptSystem& scripts = scene.AddSystem<ScriptSystem>();
        CHECK(scripts.IsRunning());
        CHECK_FALSE(scene.IsPhysicsRunning());

        scene.OnUpdate(Timestep(1.0f / 60.0f));

        CHECK(scene.IsScriptRunning());

        scene.RemoveSystem(scripts);
        CHECK_FALSE(scene.IsScriptRunning());
    }
}
