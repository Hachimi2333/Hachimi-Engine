// Physics debug drawing.
//
// The debug view is the one place where the renderer and the physics backend meet, so the parts that
// can be checked without a window are checked here: the backend's packed colour and point size turn
// into engine values, and the adapter reports the shapes of a simulated world as primitives a
// renderer could draw.
//
// No GL context is involved. DebugDraw is the GPU line renderer and is not touched; instead the
// world reports into a sink that records what it was told, which is exactly the seam the editor's
// overlay implements.

#include <doctest/doctest.h>

#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Math/Math.h"
#include "Physics/PhysicsWorld.h"
#include "Renderer/PhysicsDebugDraw.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    constexpr float Step = 1.0f / 60.0f;
    constexpr float Tolerance = 1e-3f;

    bool Near(float lhs, float rhs, float tolerance = Tolerance)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool Near(const Math::Vec3& lhs, const Math::Vec3& rhs, float tolerance = Tolerance)
    {
        return Near(lhs.x, rhs.x, tolerance) && Near(lhs.y, rhs.y, tolerance) && Near(lhs.z, rhs.z, tolerance);
    }

    // What the world reported, in the order it reported it. This is the sink the editor's overlay is,
    // with recording instead of drawing.
    struct RecordingSink final : public PhysicsDebugSink
    {
        struct Shape
        {
            enum class Kind
            {
                Box,
                Sphere,
                Capsule
            };

            Kind ShapeKind = Kind::Box;
            Math::Vec3 HalfExtents { 0.0f };
            Math::Vec3 Center { 0.0f };
            Math::Quat Rotation { 1.0f, 0.0f, 0.0f, 0.0f };
            float Radius = 0.0f;
            Math::Vec3 Start { 0.0f };
            Math::Vec3 End { 0.0f };
            Math::Vec4 Color { 0.0f };
        };

        void Segment(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color) override
        {
            Segments.push_back({ start, end });
            Colors.push_back(color);
        }

        void Point(const Math::Vec3& position, float extent, const Math::Vec4& color) override
        {
            Points.push_back(position);
            PointExtents.push_back(extent);
            Colors.push_back(color);
        }

        void Sphere(const Math::Vec3& center, float radius, const Math::Vec4& color) override
        {
            Shape shape;
            shape.ShapeKind = Shape::Kind::Sphere;
            shape.Center = center;
            shape.Radius = radius;
            shape.Color = color;
            Shapes.push_back(shape);
        }

        void Capsule(const Math::Vec3& start, const Math::Vec3& end, float radius, const Math::Vec4& color) override
        {
            Shape shape;
            shape.ShapeKind = Shape::Kind::Capsule;
            shape.Start = start;
            shape.End = end;
            shape.Radius = radius;
            shape.Color = color;
            Shapes.push_back(shape);
        }

        void Box(const Math::Vec3& halfExtents, const Math::Vec3& center, const Math::Quat& rotation,
                 const Math::Vec4& color) override
        {
            Shape shape;
            shape.ShapeKind = Shape::Kind::Box;
            shape.HalfExtents = halfExtents;
            shape.Center = center;
            shape.Rotation = rotation;
            shape.Color = color;
            Shapes.push_back(shape);
        }

        void Bounds(const Math::Vec3& minimum, const Math::Vec3& maximum, const Math::Vec4& color) override
        {
            BoundBoxes.push_back({ minimum, maximum });
            Colors.push_back(color);
        }

        size_t ShapeCountOf(Shape::Kind kind) const
        {
            size_t count = 0;
            for (const Shape& shape : Shapes)
            {
                count += shape.ShapeKind == kind ? 1 : 0;
            }
            return count;
        }

        std::vector<Shape> Shapes;
        std::vector<std::pair<Math::Vec3, Math::Vec3>> Segments;
        std::vector<Math::Vec3> Points;
        std::vector<float> PointExtents;
        std::vector<std::pair<Math::Vec3, Math::Vec3>> BoundBoxes;
        std::vector<Math::Vec4> Colors;
    };

    // A running scene with a floor and one body, stepped by hand so the suite needs no window.
    struct DebugScene
    {
        DebugScene()
        {
            for (const Entity entity : SceneUnderTest.GetAllEntities())
            {
                SceneUnderTest.DestroyEntity(entity);
            }

            SceneUnderTest.GetPhysicsSettings().FixedTimeStep = Step;
            SceneUnderTest.OnRuntimeStart(Clock);
        }

        ~DebugScene()
        {
            SceneUnderTest.OnRuntimeStop();
        }

        PhysicsWorld* World()
        {
            PhysicsSystem* physics = SceneUnderTest.FindSystem<PhysicsSystem>();
            return physics != nullptr ? physics->GetWorld() : nullptr;
        }

        void Advance(int steps = 1)
        {
            for (int index = 0; index < steps; ++index)
            {
                Clock.Advance(Step);
                SceneUnderTest.OnUpdate(Timestep(Step));
            }
        }

        Scene SceneUnderTest;
        Time Clock;
    };

    Entity AddBox(Scene& scene, const std::string& name, const Math::Vec3& position, const Math::Vec3& halfExtents)
    {
        Entity entity = scene.CreateEntity(name);
        entity.Transform().Position = position;

        entity.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;

        auto& collider = entity.AddComponent<ColliderComponent>();
        collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
        collider.HalfExtents = halfExtents;
        return entity;
    }

    Entity AddSphere(Scene& scene, const std::string& name, const Math::Vec3& position, float radius)
    {
        Entity entity = scene.CreateEntity(name);
        entity.Transform().Position = position;

        auto& rigidbody = entity.AddComponent<RigidbodyComponent>();
        rigidbody.Type = RigidbodyComponent::RigidbodyType::Static;
        rigidbody.GravityScale = 0.0f;

        auto& collider = entity.AddComponent<ColliderComponent>();
        collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
        collider.Radius = radius;
        return entity;
    }
}

TEST_SUITE("Physics")
{
    TEST_CASE("a debug colour decodes to the colour Box3D packed")
    {
        // 0xRRGGBB, the layout the backend's colour enum uses.
        const Math::Vec4 red = DecodePhysicsDebugColor(0x00FF0000u);
        CHECK(Near(red.r, 1.0f));
        CHECK(Near(red.g, 0.0f));
        CHECK(Near(red.b, 0.0f));
        CHECK(Near(red.a, 1.0f));

        const Math::Vec4 mixed = DecodePhysicsDebugColor(0x00336699u);
        CHECK(Near(mixed.r, 0x33 / 255.0f));
        CHECK(Near(mixed.g, 0x66 / 255.0f));
        CHECK(Near(mixed.b, 0x99 / 255.0f));

        // The high byte is a material preset the backend packs alongside the colour; a renderer that
        // does not implement presets must read the same RGB out of a packed colour as out of a plain
        // one, or every shape would come out with the wrong colour.
        const Math::Vec4 packed = DecodePhysicsDebugColor(0x05FF0000u);
        CHECK(Near(packed.r, red.r));
        CHECK(Near(packed.g, red.g));
        CHECK(Near(packed.b, red.b));

        CHECK(Near(DecodePhysicsDebugColor(0x00FFFFFFu, 0.25f).a, 0.25f));
    }

    TEST_CASE("a debug point size becomes a world extent")
    {
        // The backend draws contact points at 5, 10 and 20 pixels; the extent has to grow with the
        // size and stay a fraction of a metre at the scale a scene is authored in.
        CHECK(PhysicsDebugPointExtent(5.0f) > 0.0f);
        CHECK(PhysicsDebugPointExtent(20.0f) > PhysicsDebugPointExtent(5.0f));
        CHECK(Near(PhysicsDebugPointExtent(20.0f), 4.0f * PhysicsDebugPointExtent(5.0f)));

        // A size of zero or a negative one would otherwise draw a degenerate cross.
        CHECK(PhysicsDebugPointExtent(0.0f) >= 0.0f);
        CHECK(PhysicsDebugPointExtent(-5.0f) >= 0.0f);
    }

    TEST_CASE("an empty world draws nothing and does not crash")
    {
        DebugScene scene;
        scene.Advance();

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink sink;
        world->DrawDebug(sink, {});
        CHECK(sink.Shapes.empty());
        CHECK(sink.Segments.empty());
        CHECK(sink.Points.empty());
        CHECK(sink.BoundBoxes.empty());
    }

    TEST_CASE("a simulated box collider is reported as a box at its body pose")
    {
        DebugScene scene;
        AddBox(scene.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 2.0f });
        scene.Advance();

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink sink;
        world->DrawDebug(sink, {});

        REQUIRE(sink.ShapeCountOf(RecordingSink::Shape::Kind::Box) == 1);
        const RecordingSink::Shape& box = sink.Shapes.front();
        CHECK(Near(box.HalfExtents, { 4.0f, 0.5f, 2.0f }));
        CHECK(Near(box.Center, { 0.0f, 0.0f, 0.0f }));

        // A static body is drawn in the backend's own colour for one, which is not black.
        CHECK((box.Color.r + box.Color.g + box.Color.b) > 0.0f);
    }

    TEST_CASE("a box collider follows its body, offset included")
    {
        DebugScene scene;
        Entity box = AddBox(scene.SceneUnderTest, "Platform", { 2.0f, 3.0f, 0.0f }, { 1.0f, 0.25f, 0.5f });
        box.GetComponent<ColliderComponent>().Offset = { 0.5f, 0.0f, 0.0f };
        scene.Advance();

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink sink;
        world->DrawDebug(sink, {});

        REQUIRE(sink.ShapeCountOf(RecordingSink::Shape::Kind::Box) == 1);
        // The offset is baked into the shape by the body creation, so the drawn box is where the
        // collider actually is and not where the entity origin is.
        CHECK(Near(sink.Shapes.front().Center, { 2.5f, 3.0f, 0.0f }));
    }

    TEST_CASE("a sphere and a capsule keep their shape in the debug view")
    {
        DebugScene scene;
        AddSphere(scene.SceneUnderTest, "Ball", { 1.0f, 2.0f, 0.0f }, 0.75f);

        Entity capsule = scene.SceneUnderTest.CreateEntity("Pillar");
        capsule.Transform().Position = { -1.0f, 1.0f, 0.0f };
        capsule.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;
        auto& capsuleCollider = capsule.AddComponent<ColliderComponent>();
        capsuleCollider.ShapeType = ColliderComponent::ColliderShapeType::Capsule;
        capsuleCollider.Radius = 0.25f;
        capsuleCollider.Height = 2.0f;
        scene.Advance();

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink sink;
        world->DrawDebug(sink, {});

        // Shapes are reported in broad-phase visit order, so each kind is found by its own count and
        // the values are checked against the one shape of that kind.
        REQUIRE(sink.ShapeCountOf(RecordingSink::Shape::Kind::Sphere) == 1);
        REQUIRE(sink.ShapeCountOf(RecordingSink::Shape::Kind::Capsule) == 1);

        for (const RecordingSink::Shape& shape : sink.Shapes)
        {
            if (shape.ShapeKind == RecordingSink::Shape::Kind::Sphere)
            {
                CHECK(Near(shape.Center, { 1.0f, 2.0f, 0.0f }));
                CHECK(Near(shape.Radius, 0.75f));
            }
            else
            {
                // A capsule's height is the total including both caps, so the segment between the
                // cap centres is the height minus a diameter.
                CHECK(Near(shape.Radius, 0.25f));
                CHECK(Near(shape.Start.x, -1.0f));
                CHECK(Near(shape.End.x, -1.0f));
                CHECK(Near(shape.End.y - shape.Start.y, 1.5f));
                CHECK(Near(shape.Start.y + shape.End.y, 2.0f));
            }
        }
    }

    TEST_CASE("the options decide what a debug draw reports")
    {
        DebugScene scene;
        AddBox(scene.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f });
        scene.Advance();

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink shapesOnly;
        world->DrawDebug(shapesOnly, {});
        CHECK_FALSE(shapesOnly.Shapes.empty());
        CHECK(shapesOnly.BoundBoxes.empty());

        // Turning shapes off is how a caller asks for diagnostics only; nothing else draws them.
        PhysicsDebugDrawOptions noShapes;
        noShapes.Shapes = false;
        RecordingSink nothing;
        world->DrawDebug(nothing, noShapes);
        CHECK(nothing.Shapes.empty());

        PhysicsDebugDrawOptions withBounds;
        withBounds.Bounds = true;
        RecordingSink bounded;
        world->DrawDebug(bounded, withBounds);
        CHECK_FALSE(bounded.BoundBoxes.empty());
        CHECK(bounded.Shapes.size() == shapesOnly.Shapes.size());
        for (const auto& [minimum, maximum] : bounded.BoundBoxes)
        {
            CHECK(minimum.x <= maximum.x);
            CHECK(minimum.y <= maximum.y);
            CHECK(minimum.z <= maximum.z);
        }
    }

    TEST_CASE("contacts are reported as points and normals")
    {
        DebugScene scene;
        AddBox(scene.SceneUnderTest, "Floor", { 0.0f, 0.0f, 0.0f }, { 4.0f, 0.5f, 4.0f });

        // A body that never sleeps keeps its contacts drawn, so the case does not depend on when the
        // simulation decides the ball has settled.
        Entity ball = AddSphere(scene.SceneUnderTest, "Ball", { 0.0f, 1.5f, 0.0f }, 0.5f);
        auto& rigidbody = ball.GetComponent<RigidbodyComponent>();
        rigidbody.Type = RigidbodyComponent::RigidbodyType::Dynamic;
        rigidbody.EnableSleep = false;
        rigidbody.GravityScale = 1.0f;
        scene.Advance(30);

        PhysicsWorld* world = scene.World();
        REQUIRE(world != nullptr);

        RecordingSink withoutContacts;
        world->DrawDebug(withoutContacts, {});
        CHECK(withoutContacts.Points.empty());

        PhysicsDebugDrawOptions withContacts;
        withContacts.Contacts = true;
        RecordingSink sink;
        world->DrawDebug(sink, withContacts);

        REQUIRE_FALSE(sink.Points.empty());
        CHECK_FALSE(sink.Segments.empty());

        // Points arrive with the backend's pixel size already turned into a world extent, and the
        // sizes it uses are 5, 10, 15 and 20.
        for (const float extent : sink.PointExtents)
        {
            const bool known =
                Near(extent, PhysicsDebugPointExtent(5.0f))
                || Near(extent, PhysicsDebugPointExtent(10.0f))
                || Near(extent, PhysicsDebugPointExtent(15.0f))
                || Near(extent, PhysicsDebugPointExtent(20.0f));
            CHECK(known);
        }

        // The contact is between a ball resting around y = 0.5 and the floor, so every point is near
        // the surface rather than at the origin.
        for (const Math::Vec3& point : sink.Points)
        {
            CHECK(point.y > 0.2f);
            CHECK(point.y < 1.6f);
        }
    }
}
