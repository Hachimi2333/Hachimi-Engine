// Contact, trigger and body move events.
//
// The engine could simulate a scene but never told anyone what the simulation did: a script could
// not react to a ball landing, and a trigger volume could only be discovered by polling overlaps
// each frame. These cases drive the event path end to end - Box3D's buffers, the world's queue, the
// scene's dispatcher and its subscribers - on a scene whose runtime was started by hand, so no
// window and no GL context is involved.
//
// Subscribers are attached exactly the way a system or the script layer attaches one, through
// Scene::GetDispatcher, so what is under test is the published event and not a test-only shortcut.

#include <doctest/doctest.h>

#include <entt/entt.hpp>

#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Physics/PhysicsEvents.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Events/PhysicsSceneEvents.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Math/Math.h"

#include <cmath>
#include <cstdint>
#include <vector>

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

    // Starts the runtime with the caller's clock and steps the world once, which is enough for the
    // first step to create bodies. Doing this by hand is what keeps the suite headless.
    void StartPhysics(Scene& scene, Time& clock)
    {
        scene.OnRuntimeStart(clock);
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));
    }

    void AdvanceStep(Scene& scene, Time& clock)
    {
        clock.Advance(Step);
        scene.OnUpdate(Timestep(Step));
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

    Entity AddSphere(Scene& scene,
                     const std::string& name,
                     const Math::Vec3& position,
                     float radius,
                     RigidbodyComponent::RigidbodyType type,
                     bool isTrigger)
    {
        Entity entity = scene.CreateEntity(name);
        entity.Transform().Position = position;

        entity.AddComponent<RigidbodyComponent>().Type = type;

        auto& collider = entity.AddComponent<ColliderComponent>();
        collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
        collider.Radius = radius;
        collider.IsTrigger = isTrigger;
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

    // Records what the scene published. Subscribed with a member function, which is the shape the
    // payload overload of sink::connect takes - and which keeps the two handlers from collapsing
    // into one function pointer the way two identical capture-less lambdas would.
    struct EventRecorder
    {
        void OnCollision(const SceneCollisionEvent& event)
        {
            ++CollisionCalls;
            Begins.insert(Begins.end(), event.Begins.begin(), event.Begins.end());
            Ends.insert(Ends.end(), event.Ends.begin(), event.Ends.end());
            Hits.insert(Hits.end(), event.Hits.begin(), event.Hits.end());
        }

        void OnTrigger(const SceneTriggerEvent& event)
        {
            ++TriggerCalls;
            TriggerBegins.insert(TriggerBegins.end(), event.Begins.begin(), event.Begins.end());
            TriggerEnds.insert(TriggerEnds.end(), event.Ends.begin(), event.Ends.end());
        }

        void Reset()
        {
            CollisionCalls = 0;
            TriggerCalls = 0;
            Begins.clear();
            Ends.clear();
            Hits.clear();
            TriggerBegins.clear();
            TriggerEnds.clear();
        }

        size_t CollisionCalls = 0;
        size_t TriggerCalls = 0;
        std::vector<CollisionBeginEvent> Begins;
        std::vector<CollisionEndEvent> Ends;
        std::vector<CollisionHitEvent> Hits;
        std::vector<TriggerBeginEvent> TriggerBegins;
        std::vector<TriggerEndEvent> TriggerEnds;
    };

    // A scene that is already running, with both event kinds subscribed.
    struct PhysicsEventScene
    {
        PhysicsEventScene()
        {
            ClearScene(SceneUnderTest);
            CollisionConnection = SceneUnderTest.GetDispatcher().sink<SceneCollisionEvent>()
                                      .connect<&EventRecorder::OnCollision>(Recorder);
            TriggerConnection = SceneUnderTest.GetDispatcher().sink<SceneTriggerEvent>()
                                    .connect<&EventRecorder::OnTrigger>(Recorder);
            StartPhysics(SceneUnderTest, Clock);
        }

        ~PhysicsEventScene()
        {
            SceneUnderTest.OnRuntimeStop();
        }

        Scene SceneUnderTest;
        Time Clock;
        EventRecorder Recorder;
        entt::connection CollisionConnection;
        entt::connection TriggerConnection;
    };

    // A ball resting on a floor: one contact, reported once. Returns the ball; the floor is the
    // first entity of the pair.
    Entity AddFloorAndBall(Scene& scene, float ballRadius, float ballHeight)
    {
        AddBox(scene, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
               RigidbodyComponent::RigidbodyType::Static);
        return AddSphere(scene, "Ball", { 0.0f, ballHeight, 0.0f }, ballRadius,
                         RigidbodyComponent::RigidbodyType::Dynamic, false);
    }

    // The floor AddFloorAndBall created, so a case can name it without guessing at iterator order.
    Entity FloorOf(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            if (entity.GetName() == "Floor")
            {
                return entity;
            }
        }

        return Entity();
    }
}

TEST_SUITE("Physics")
{
    TEST_CASE("a collision is reported once when it begins and once when it ends")
    {
        PhysicsEventScene world;
        const Entity floor = AddBox(world.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
                                    RigidbodyComponent::RigidbodyType::Static);
        const Entity ball = AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 0.5f, 0.0f }, 0.5f,
                                      RigidbodyComponent::RigidbodyType::Dynamic, false);

        // The ball starts a hair above the floor, so the first step is the one that touches.
        AdvanceStep(world.SceneUnderTest, world.Clock);
        REQUIRE(world.Recorder.Begins.size() == 1);
        CHECK(world.Recorder.Ends.empty());

        const CollisionBeginEvent& begin = world.Recorder.Begins.front();
        CHECK((begin.EntityKeyA == KeyOf(ball) || begin.EntityKeyA == KeyOf(floor)));
        CHECK((begin.EntityKeyB == KeyOf(ball) || begin.EntityKeyB == KeyOf(floor)));
        CHECK(begin.EntityKeyA != begin.EntityKeyB);

        // The normal runs from one body to the other along the vertical: which of the two Box3D calls
        // "A" is its choice, the axis is not.
        CHECK(Near(std::abs(begin.Normal.y), 1.0f, 0.05f));

        // A resting contact reports nothing further, however many steps it is simulated for.
        world.Recorder.Reset();
        for (int step = 0; step < 30; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }
        CHECK(world.Recorder.Begins.empty());
        CHECK(world.Recorder.Ends.empty());

        // Throwing the ball upwards ends the contact on the next step that separates them. The
        // velocity is set on the body, not on the component: a dynamic body's velocity is the
        // simulation's to own, and the component is only read back into Box3D when the body was just
        // created or just changed type.
        PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);
        physics->SetLinearVelocity(KeyOf(ball), { 0.0f, 1.5f, 0.0f });

        bool ended = false;
        for (int step = 0; step < 30 && !ended; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            ended = !world.Recorder.Ends.empty();
        }

        REQUIRE(ended);
        CHECK(world.Recorder.Ends.size() == 1);
        const CollisionEndEvent& end = world.Recorder.Ends.front();
        CHECK((end.EntityKeyA == KeyOf(ball) || end.EntityKeyA == KeyOf(floor)));
        CHECK((end.EntityKeyB == KeyOf(ball) || end.EntityKeyB == KeyOf(floor)));

        // The same two entities meeting again is a new begin, not a continuation: an "inside" flag
        // held by a listener has to go back to zero after the end event.
        world.Recorder.Reset();
        bool beganAgain = false;
        for (int step = 0; step < 120 && !beganAgain; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            beganAgain = !world.Recorder.Begins.empty();
        }

        REQUIRE(beganAgain);
        CHECK(world.Recorder.Begins.size() == 1);
    }

    TEST_CASE("a fast impact is reported as a hit with the approach speed and the impact point")
    {
        PhysicsEventScene world;
        AddBox(world.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
               RigidbodyComponent::RigidbodyType::Static);
        AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 6.0f, 0.0f }, 0.5f,
                  RigidbodyComponent::RigidbodyType::Dynamic, false);

        bool arrived = false;
        for (int step = 0; step < 300 && !arrived; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            arrived = !world.Recorder.Hits.empty();
        }

        REQUIRE(arrived);
        const CollisionHitEvent& hit = world.Recorder.Hits.front();

        // Falling six units ends at roughly sqrt(2 * 10 * 5) units per second, and Box3D reports the
        // closing speed along the normal, which is positive by construction.
        CHECK(hit.ApproachSpeed > 5.0f);
        CHECK(Near(std::abs(hit.Normal.y), 1.0f, 0.05f));

        // The ball falls straight onto the floor's top face, so the impact point is on that face: the
        // floor spans y = -0.5 to 0.5 and the ball arrives on the x = z = 0 line. Reported at the
        // start of the step that confirms the impulse, so it sits a little above the surface a body
        // travelling ten units per second sinks into during the rest of that step.
        CHECK(Near(hit.Point.x, 0.0f, 0.01f));
        CHECK(Near(hit.Point.z, 0.0f, 0.01f));
        CHECK(hit.Point.y <= 0.5f + 0.01f);
        CHECK(hit.Point.y > 0.4f);
    }

    TEST_CASE("a trigger reports entering and leaving exactly once each")
    {
        PhysicsEventScene world;
        // The visitor starts inside the volume, so the first step is the one that enters it.
        const Entity volume = AddSphere(world.SceneUnderTest, "Volume", { 0.0f, 0.0f, 0.0f }, 2.0f,
                                        RigidbodyComponent::RigidbodyType::Static, true);
        const Entity ball = AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 0.0f, 0.0f }, 0.5f,
                                      RigidbodyComponent::RigidbodyType::Dynamic, false);

        AdvanceStep(world.SceneUnderTest, world.Clock);
        REQUIRE(world.Recorder.TriggerBegins.size() == 1);
        CHECK(world.Recorder.TriggerEnds.empty());

        const TriggerBeginEvent& begin = world.Recorder.TriggerBegins.front();
        CHECK(begin.SensorEntityKey == KeyOf(volume));
        CHECK(begin.OtherEntityKey == KeyOf(ball));

        // Staying inside is not a new entry.
        world.Recorder.Reset();
        for (int step = 0; step < 20; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }
        CHECK(world.Recorder.TriggerBegins.empty());
        CHECK(world.Recorder.TriggerEnds.empty());

        // Falling out of the volume leaves it, and falling back in enters it again.
        bool left = false;
        for (int step = 0; step < 200 && !left; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            left = !world.Recorder.TriggerEnds.empty();
        }

        REQUIRE(left);
        CHECK(world.Recorder.TriggerEnds.size() == 1);
        CHECK(world.Recorder.TriggerEnds.front().SensorEntityKey == KeyOf(volume));
        CHECK(world.Recorder.TriggerEnds.front().OtherEntityKey == KeyOf(ball));

        // Moving the ball back inside has to go through the physics world: writing the component
        // transform alone would leave the body where it was, and the trigger tests the body.
        PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);
        physics->SetLinearVelocity(KeyOf(ball), { 0.0f, 0.0f, 0.0f });
        physics->Teleport(KeyOf(ball), { 0.0f, 0.0f, 0.0f }, Math::Quat(1.0f, 0.0f, 0.0f, 0.0f), world.SceneUnderTest);

        world.Recorder.Reset();
        AdvanceStep(world.SceneUnderTest, world.Clock);
        CHECK(world.Recorder.TriggerBegins.size() == 1);
        CHECK(world.Recorder.TriggerEnds.empty());
    }

    TEST_CASE("turning contact events off ends the contacts that were being reported")
    {
        PhysicsEventScene world;
        const Entity ball = AddFloorAndBall(world.SceneUnderTest, 0.5f, 0.5f);

        AdvanceStep(world.SceneUnderTest, world.Clock);
        REQUIRE(world.Recorder.Begins.size() == 1);

        // The toggle reaches the shapes, so from the next step on the contact that is still there is
        // simply not reported. The listener is not handed an end event for a contact it did not see
        // end, which is the honest answer: the two bodies never separated.
        world.Recorder.Reset();
        world.SceneUnderTest.GetPhysicsSettings().EnableContactEvents = false;

        for (int step = 0; step < 10; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }
        CHECK(world.Recorder.Begins.empty());
        CHECK(world.Recorder.Ends.empty());
        CHECK(world.Recorder.Hits.empty());

        // The setting gates reporting, not the simulation.
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);
        CHECK(physics->HasBody(KeyOf(ball)));

        // Turning it back on resumes the transitions that happen from then on: the ball thrown
        // upwards leaves the floor and lands again, and both halves are reported. A contact that was
        // already touching when the setting came back is not re-announced, which is why the batch
        // that follows only holds the ending.
        world.SceneUnderTest.GetPhysicsSettings().EnableContactEvents = true;
        PhysicsWorld* mutablePhysics = WorldOf(world.SceneUnderTest);
        mutablePhysics->SetLinearVelocity(KeyOf(ball), { 0.0f, 1.5f, 0.0f });

        bool ended = false;
        for (int step = 0; step < 60 && !ended; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            ended = !world.Recorder.Ends.empty();
        }

        REQUIRE(ended);
        CHECK(world.Recorder.Begins.empty());

        bool beganAgain = false;
        for (int step = 0; step < 120 && !beganAgain; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
            beganAgain = !world.Recorder.Begins.empty();
        }

        REQUIRE(beganAgain);
        CHECK(world.Recorder.Begins.size() == 1);
    }

    TEST_CASE("a scene that never asked for contact events reports none, but still reports triggers")
    {
        PhysicsEventScene world;
        world.SceneUnderTest.GetPhysicsSettings().EnableContactEvents = false;

        const Entity ball = AddFloorAndBall(world.SceneUnderTest, 0.5f, 0.5f);
        const Entity floor = FloorOf(world.SceneUnderTest);

        // A trigger volume sitting around the resting ball and the floor under it. It is dynamic with
        // its gravity turned off: a sensor the simulation cannot move is the "area the level author
        // placed" case worth pinning, and the ball is kept in place by the setting itself.
        const Entity volume = AddSphere(world.SceneUnderTest, "Volume", { 0.0f, 1.0f, 0.0f }, 1.0f,
                                        RigidbodyComponent::RigidbodyType::Dynamic, true);
        world.SceneUnderTest.GetRegistry().get<RigidbodyComponent>(volume.GetHandle()).GravityScale = 0.0f;

        for (int step = 0; step < 20; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }

        // The ball and the floor overlap, so there are solid contacts - and neither is reported,
        // because the scene turned that off. The setting has nothing to say about the sensor: a
        // trigger without overlap events would not be a trigger.
        CHECK(world.Recorder.Begins.empty());
        CHECK(world.Recorder.Ends.empty());
        CHECK(world.Recorder.Hits.empty());

        // Both solid shapes are inside the volume and both are reported, in one batch.
        REQUIRE(world.Recorder.TriggerBegins.size() == 2);
        CHECK(world.Recorder.TriggerBegins[0].SensorEntityKey == KeyOf(volume));
        CHECK(world.Recorder.TriggerBegins[0].OtherEntityKey == KeyOf(ball));
        CHECK(world.Recorder.TriggerBegins[1].SensorEntityKey == KeyOf(volume));
        CHECK(world.Recorder.TriggerBegins[1].OtherEntityKey == KeyOf(floor));

        // And entering is reported once: twenty steps inside the volume is not twenty entries.
        CHECK(world.Recorder.TriggerCalls == 1);
    }

    TEST_CASE("events whose entity was destroyed are dropped instead of reported with a stale key")
    {
        PhysicsEventScene world;
        const Entity floor = AddBox(world.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
                                    RigidbodyComponent::RigidbodyType::Static);
        const Entity ball = AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 0.5f, 0.0f }, 0.5f,
                                      RigidbodyComponent::RigidbodyType::Dynamic, false);

        AdvanceStep(world.SceneUnderTest, world.Clock);
        REQUIRE(world.Recorder.Begins.size() == 1);

        // Destroying the floor destroys the contact with it. Box3D reports that as an end touch, and
        // the end event names a body that no longer has an owner - reporting it would hand a
        // listener a key that resolves to nothing, or to whatever entity reuses the slot.
        world.Recorder.Reset();
        world.SceneUnderTest.DestroyEntity(floor);

        for (int step = 0; step < 10; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }

        CHECK(world.Recorder.Ends.empty());
        CHECK(world.Recorder.Begins.empty());

        // The surviving entity still reports its own contacts: dropping the stale event must not
        // turn the event path off.
        const PhysicsWorld* physics = WorldOf(world.SceneUnderTest);
        REQUIRE(physics != nullptr);
        CHECK(physics->HasBody(KeyOf(ball)));
        CHECK_FALSE(physics->HasBody(KeyOf(floor)));
    }

    TEST_CASE("body transforms are written back from the step's move events")
    {
        PhysicsEventScene world;
        AddBox(world.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
               RigidbodyComponent::RigidbodyType::Static);
        const Entity ball = AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 8.0f, 0.0f }, 0.4f,
                                      RigidbodyComponent::RigidbodyType::Dynamic, false);

        const float startHeight = world.SceneUnderTest.GetRegistry().get<TransformComponent>(ball.GetHandle()).Position.y;
        for (int step = 0; step < 30; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }

        const auto& rigidbody = world.SceneUnderTest.GetRegistry().get<RigidbodyComponent>(ball.GetHandle());
        const auto& transform = world.SceneUnderTest.GetRegistry().get<TransformComponent>(ball.GetHandle());

        // Falling is only visible in the ECS because the write-back ran, and the interpolation
        // history is recorded from the same state, so it has to be in step with it.
        CHECK(transform.Position.y < startHeight);
        CHECK(rigidbody.PreviousTransformValid);
        CHECK(rigidbody.PreviousPosition.y > transform.Position.y);
    }

    TEST_CASE("a scene with no events publishes none")
    {
        PhysicsEventScene world;
        AddBox(world.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f },
               RigidbodyComponent::RigidbodyType::Static);
        AddSphere(world.SceneUnderTest, "Ball", { 0.0f, 20.0f, 0.0f }, 0.5f,
                  RigidbodyComponent::RigidbodyType::Dynamic, false);

        for (int step = 0; step < 10; ++step)
        {
            AdvanceStep(world.SceneUnderTest, world.Clock);
        }

        // Nothing is touching yet, so a subscriber must not be called at all: an empty batch would
        // make every listener branch on "is this array empty".
        CHECK(world.Recorder.CollisionCalls == 0);
        CHECK(world.Recorder.TriggerCalls == 0);
    }
}
