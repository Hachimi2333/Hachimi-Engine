// Scene's fixed step: how many times FixedUpdate runs per frame, and the interpolation alpha.
//
// Physics used to keep its own accumulator and step inside PhysicsWorld::Update, so nothing else in
// the scene could run at the same rate: a script's FixedUpdate and a physics step were two separate
// loops that drifted apart as soon as either clamped differently. The accumulator now lives in
// Core/Time.h, which the main loop advances once per frame, and Scene::OnUpdate only drains it.
// These cases pin that from outside: how many steps a frame owes, what a paused frame does, how the
// frame clamp bounds the work, and what alpha a renderer is handed.
//
// Scene::OnRuntimeStart takes the clock it should use, so every case here owns a local Time and
// never depends on the process-wide one: no window, no GL context, no cross-test state.

#include <doctest/doctest.h>

#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Renderer/RenderView.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSystem.h"

#include <cmath>
#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    constexpr float Step60 = 1.0f / 60.0f;

    // Counts FixedUpdate passes and remembers the delta each one received.
    class FixedStepProbe final : public SceneSystem
    {
    public:
        std::string_view GetName() const override { return "FixedStepProbe"; }
        ScenePhase GetPhase() const override { return ScenePhase::FixedUpdate; }

        void OnUpdate(Scene& scene, Timestep timestep) override
        {
            (void)scene;
            ++Steps;
            LastDelta = timestep.GetSeconds();
        }

        int Steps = 0;
        float LastDelta = 0.0f;
    };

    class UpdateProbe final : public SceneSystem
    {
    public:
        std::string_view GetName() const override { return "UpdateProbe"; }
        ScenePhase GetPhase() const override { return ScenePhase::Update; }

        void OnUpdate(Scene& scene, Timestep timestep) override
        {
            (void)scene;
            LastDelta = timestep.GetSeconds();
        }

        float LastDelta = 0.0f;
    };

    // Frame order, so the phase sequence can be asserted without a simulation.
    class OrderProbe final : public SceneSystem
    {
    public:
        OrderProbe(ScenePhase phase, std::vector<std::string>& log)
            : m_Phase(phase)
            , m_Log(log)
        {
        }

        std::string_view GetName() const override { return "OrderProbe"; }
        ScenePhase GetPhase() const override { return m_Phase; }

        void OnUpdate(Scene& scene, Timestep timestep) override
        {
            (void)scene;
            (void)timestep;
            m_Log.push_back(m_Phase == ScenePhase::PreUpdate ? "pre"
                : m_Phase == ScenePhase::FixedUpdate     ? "fixed"
                : m_Phase == ScenePhase::Update          ? "update"
                                                         : "late");
        }

    private:
        ScenePhase m_Phase;
        std::vector<std::string>& m_Log;
    };

    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
    }

    // A floor plus a dynamic cube, which is the smallest scene that actually moves.
    Entity AddFallingCube(Scene& scene)
    {
        Entity ground = scene.CreateEntity("Ground");
        ground.Transform().Position = { 0.0f, -0.5f, 0.0f };
        ground.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;
        auto& groundCollider = ground.AddComponent<ColliderComponent>();
        groundCollider.ShapeType = ColliderComponent::ColliderShapeType::Box;
        groundCollider.HalfExtents = { 5.0f, 0.5f, 5.0f };

        Entity cube = scene.CreateEntity("Cube");
        cube.Transform().Position = { 0.0f, 4.0f, 0.0f };
        cube.AddComponent<RigidbodyComponent>();
        cube.AddComponent<ColliderComponent>();

        return cube;
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    float TranslationY(const Math::Mat4& transform)
    {
        return transform[3].y;
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("one frame of exactly one fixed step runs FixedUpdate once")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        clock.Advance(Step60);
        scene.OnUpdate(Timestep(Step60));

        CHECK(probe.Steps == 1);
        CHECK(Near(probe.LastDelta, Step60));
        CHECK(Near(scene.GetFixedDeltaTime(), Step60));

        scene.OnRuntimeStop();
    }

    TEST_CASE("no fixed step is owed before a whole step of time has passed")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        // Two frames that together add up to less than one step.
        clock.Advance(Step60 * 0.5f);
        scene.OnUpdate(Timestep(Step60 * 0.5f));
        CHECK(probe.Steps == 0);
        CHECK(Near(scene.GetInterpolationAlpha(), 0.5f));

        clock.Advance(Step60 * 0.5f);
        scene.OnUpdate(Timestep(Step60 * 0.5f));

        // The second half completed the step, so this frame ran it.
        CHECK(probe.Steps == 1);
        CHECK(Near(scene.GetInterpolationAlpha(), 0.0f));

        scene.OnRuntimeStop();
    }

    TEST_CASE("a long frame owes several steps but never more than the catch-up cap")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        // One frame of 70 ms with a 60 Hz step: roughly four steps' worth, against a cap of four.
        clock.Advance(0.07f);
        scene.OnUpdate(Timestep(0.07f));

        CHECK(probe.Steps == Time::MaxStepsPerFrame);

        // A frame whose debt reaches the cap leaves nothing to interpolate with. Time drops the
        // surplus time rather than carrying it, because carrying it is what turns one slow frame
        // into a run of slow frames. Alpha 0 means "the last step is the truth", which is the
        // correct answer for a frame that could not keep up.
        CHECK(Near(clock.GetStepAccumulator(), 0.0f));
        CHECK(Near(scene.GetInterpolationAlpha(), 0.0f));

        scene.OnRuntimeStop();
    }

    TEST_CASE("one OnUpdate drains the whole frame, leaving only the leftover fraction")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        // A step that is exact in binary, so the arithmetic below is about the accumulator and not
        // about 0.01f not being 0.01f. The scene takes its fixed step from its own physics settings
        // when the runtime starts, so both sides are set.
        constexpr float ExactStep = 1.0f / 16.0f;
        scene.GetPhysicsSettings().FixedTimeStep = ExactStep;

        Time clock;
        clock.SetFixedDeltaTime(ExactStep);
        scene.OnRuntimeStart(clock);

        // Two frames of exactly one step each. Within a frame, OnUpdate drains what the clock holds
        // and leaves the fraction behind for the next one.
        float elapsed = 0.0f;
        for (int frame = 0; frame < 2; ++frame)
        {
            clock.Advance(ExactStep);
            elapsed += ExactStep;
            scene.OnUpdate(Timestep(ExactStep));
        }

        CHECK(probe.Steps == 2);
        CHECK(Near(clock.GetStepAccumulator(), 0.0f));

        clock.Advance(ExactStep * 0.5f);
        elapsed += ExactStep * 0.5f;
        scene.OnUpdate(Timestep(ExactStep * 0.5f));

        CHECK(probe.Steps == 2);
        CHECK(Near(scene.GetInterpolationAlpha(), 0.5f));

        // The half step was carried rather than dropped, so half a step more completes it.
        clock.Advance(ExactStep * 0.5f);
        elapsed += ExactStep * 0.5f;
        scene.OnUpdate(Timestep(ExactStep * 0.5f));

        CHECK(probe.Steps == 3);
        CHECK(Near(scene.GetInterpolationAlpha(), 0.0f));

        // Stepped time matches the time the clock was given: no step was invented and none was lost.
        CHECK(Near(static_cast<float>(probe.Steps) * ExactStep, elapsed));

        scene.OnRuntimeStop();
    }

    TEST_CASE("the frame clamp is what bounds the work one OnUpdate can owe")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        constexpr float ExactStep = 1.0f / 16.0f;
        scene.GetPhysicsSettings().FixedTimeStep = ExactStep;

        Time clock;
        clock.SetFixedDeltaTime(ExactStep);
        scene.OnRuntimeStart(clock);

        // A frame nine times longer than the step. Time clamps the frame to MaxRawDelta, so the whole
        // frame is worth well under the catch-up cap no matter how long the real frame was, and the
        // fixed phase cannot be asked for more steps than MaxStepsPerFrame in one OnUpdate.
        clock.Advance(ExactStep * 9.0f);
        REQUIRE(Near(clock.GetStepAccumulator(), Time::MaxRawDelta));

        scene.OnUpdate(Timestep(ExactStep * 9.0f));

        CHECK(probe.Steps == 1);

        // The clamped frame is 0.1s against a 0.0625s step, so a step and a fraction of one ran.
        CHECK(clock.GetStepAccumulator() < clock.GetFixedDeltaTime());
        CHECK(scene.GetInterpolationAlpha() >= 0.0f);
        CHECK(scene.GetInterpolationAlpha() <= 1.0f);

        scene.OnRuntimeStop();
    }

    TEST_CASE("a zero time scale freezes the fixed step while the frame still runs")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();
        UpdateProbe& updateProbe = scene.AddSystem<UpdateProbe>();

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);
        clock.SetTimeScale(0.0f);

        for (int frame = 0; frame < 8; ++frame)
        {
            clock.Advance(Step60);
            scene.OnUpdate(Timestep(Step60));
        }

        CHECK(probe.Steps == 0);
        CHECK(Near(scene.GetInterpolationAlpha(), 0.0f));

        // The variable phase still runs, with no simulated time at all: that is what keeps a paused
        // scene's UI alive.
        CHECK(Near(updateProbe.LastDelta, 0.0f));
        CHECK(scene.IsRuntimeRunning());

        scene.OnRuntimeStop();
    }

    TEST_CASE("stopping the runtime drops the time it still owed")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        // Almost a step of debt, then a stop.
        clock.Advance(Step60 * 0.75f);
        scene.OnUpdate(Timestep(Step60 * 0.75f));
        CHECK(probe.Steps == 0);

        scene.OnRuntimeStop();
        CHECK(Near(clock.GetStepAccumulator(), 0.0f));
        CHECK_FALSE(scene.IsRuntimeRunning());
        CHECK(Near(scene.GetFixedDeltaTime(), 0.0f));
        CHECK(Near(scene.GetInterpolationAlpha(), 1.0f));

        // Starting again does not run the step the previous run was holding.
        scene.OnRuntimeStart(clock);
        CHECK(probe.Steps == 0);

        clock.Advance(Step60 * 0.75f);
        scene.OnUpdate(Timestep(Step60 * 0.75f));
        CHECK(probe.Steps == 0);

        scene.OnRuntimeStop();
    }

    TEST_CASE("a scene that is not running never enters the fixed update phase")
    {
        Scene scene;
        ClearScene(scene);
        FixedStepProbe& probe = scene.AddSystem<FixedStepProbe>();
        UpdateProbe& updateProbe = scene.AddSystem<UpdateProbe>();

        // No runtime: this is the editor in edit mode. The fixed phase must stay out of the way
        // even when plenty of time has passed.
        for (int frame = 0; frame < 10; ++frame)
        {
            scene.OnUpdate(Timestep(1.0f));
        }

        CHECK(probe.Steps == 0);
        CHECK(Near(updateProbe.LastDelta, 1.0f));

        // Alpha 1 disables interpolation, so the picture is exactly the stored transform.
        CHECK(Near(scene.GetInterpolationAlpha(), 1.0f));
        CHECK(Near(scene.GetFixedDeltaTime(), 0.0f));
    }

    TEST_CASE("the phases of a running scene run pre, fixed, update, late")
    {
        Scene scene;
        ClearScene(scene);

        std::vector<std::string> log;
        scene.AddSystem<OrderProbe>(ScenePhase::LateUpdate, log);
        scene.AddSystem<OrderProbe>(ScenePhase::Update, log);
        scene.AddSystem<OrderProbe>(ScenePhase::FixedUpdate, log);
        scene.AddSystem<OrderProbe>(ScenePhase::PreUpdate, log);

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        clock.Advance(Step60);
        scene.OnUpdate(Timestep(Step60));

        REQUIRE(log.size() == 4);
        CHECK(log[0] == "pre");
        CHECK(log[1] == "fixed");
        CHECK(log[2] == "update");
        CHECK(log[3] == "late");

        // Two owed steps mean the fixed phase runs twice, in place: the order around it is unchanged
        // and the work happens as a block rather than interleaved with the variable phases.
        log.clear();
        clock.Advance(Step60 * 2.0f);
        scene.OnUpdate(Timestep(Step60 * 2.0f));

        REQUIRE(log.size() == 5);
        CHECK(log[0] == "pre");
        CHECK(log[1] == "fixed");
        CHECK(log[2] == "fixed");
        CHECK(log[3] == "update");
        CHECK(log[4] == "late");

        scene.OnRuntimeStop();
    }

    TEST_CASE("a dynamic body only moves on the fixed step, not on the frame")
    {
        Scene scene;
        ClearScene(scene);
        Entity cube = AddFallingCube(scene);
        const float startY = cube.Transform().Position.y;

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        // A frame worth less than a step: the simulation must not advance at all.
        clock.Advance(Step60 * 0.25f);
        scene.OnUpdate(Timestep(Step60 * 0.25f));
        CHECK(Near(cube.Transform().Position.y, startY));

        // Four steps of simulated time, and the body has fallen.
        clock.Advance(Step60 * 4.0f);
        scene.OnUpdate(Timestep(Step60 * 4.0f));
        CHECK(cube.Transform().Position.y < startY);

        scene.OnRuntimeStop();
    }

    TEST_CASE("a stopped scene's render item is the stored transform, not an interpolation")
    {
        Scene scene;
        ClearScene(scene);
        Entity cube = AddFallingCube(scene);
        cube.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);

        const RenderView view = scene.BuildRenderView(SceneRenderDesc {});
        REQUIRE(view.Items.size() == 1);

        // With the runtime stopped alpha is 1, so the item is exactly where the scene puts it.
        CHECK(Near(TranslationY(view.Items[0].Transform), cube.Transform().Position.y));
    }

    TEST_CASE("a stepped body is drawn between its last two simulated positions")
    {
        Scene scene;
        ClearScene(scene);
        Entity cube = AddFallingCube(scene);
        cube.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        clock.Advance(Step60);
        scene.OnUpdate(Timestep(Step60));

        const auto& rigidbody = cube.GetComponent<RigidbodyComponent>();
        REQUIRE(rigidbody.PreviousTransformValid);

        const float previous = rigidbody.PreviousPosition.y;
        const float current = cube.Transform().Position.y;
        REQUIRE(previous > current);

        // Half a step of debt: halfway between the last two simulated positions.
        clock.Advance(Step60 * 0.5f);
        scene.OnUpdate(Timestep(Step60 * 0.5f));
        REQUIRE(Near(scene.GetInterpolationAlpha(), 0.5f));

        const RenderView view = scene.BuildRenderView(SceneRenderDesc {});
        REQUIRE(view.Items.size() == 1);

        const float drawn = TranslationY(view.Items[0].Transform);
        CHECK(Near(drawn, (previous + cube.Transform().Position.y) * 0.5f, 1e-3f));

        // Never ahead of the state being simulated: interpolation must not extrapolate.
        CHECK(drawn <= previous);
        CHECK(drawn >= cube.Transform().Position.y);

        scene.OnRuntimeStop();

        // Stopping restores the exact stored transform.
        const RenderView stopped = scene.BuildRenderView(SceneRenderDesc {});
        REQUIRE(stopped.Items.size() == 1);
        CHECK(Near(TranslationY(stopped.Items[0].Transform), cube.Transform().Position.y));
    }

    TEST_CASE("a parented body interpolates in its parent's space")
    {
        Scene scene;
        ClearScene(scene);

        Entity parent = scene.CreateEntity("Parent");
        parent.Transform().Position = { 10.0f, 0.0f, 0.0f };

        Entity cube = AddFallingCube(scene);
        cube.Transform().Position = { 0.0f, 4.0f, 0.0f };
        scene.SetParent(cube, parent);
        cube.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);

        Time clock;
        clock.SetFixedDeltaTime(Step60);
        scene.OnRuntimeStart(clock);

        clock.Advance(Step60);
        scene.OnUpdate(Timestep(Step60));

        const auto& rigidbody = cube.GetComponent<RigidbodyComponent>();
        REQUIRE(rigidbody.PreviousTransformValid);

        clock.Advance(Step60 * 0.5f);
        scene.OnUpdate(Timestep(Step60 * 0.5f));

        const RenderView view = scene.BuildRenderView(SceneRenderDesc {});
        REQUIRE(view.Items.size() == 1);

        // The parent offset survives, so the blend happened in local space rather than being
        // applied to a world position and then re-parented.
        CHECK(Near(view.Items[0].Transform[3].x, 10.0f, 1e-3f));

        const float expectedY = (rigidbody.PreviousPosition.y + cube.Transform().Position.y) * 0.5f;
        CHECK(Near(TranslationY(view.Items[0].Transform), expectedY, 1e-3f));

        scene.OnRuntimeStop();
    }
}
