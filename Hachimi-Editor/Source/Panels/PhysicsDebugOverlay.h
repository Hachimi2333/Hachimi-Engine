#pragma once

#include "Core/Base.h"
#include "Renderer/PhysicsDebugDraw.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    class DebugDraw;
    class Scene;
    class PhysicsWorld;
    struct EditorContext;

    // Draws the physics debug view into the viewport.
    //
    // The editor has two states and they have different sources of truth, so this draws from
    // whichever one exists:
    //
    //   Playing          the running world answers through PhysicsWorld::DrawDebug, which is where
    //                    the collider poses, the broad-phase bounds and the contact points of the
    //                    live simulation come from.
    //   Editing          there is no world - nothing is simulated - so the authored colliders are
    //                    drawn from the scene's own components. That is the view a collider toggle is
    //                    for: seeing what a shape is before anything moves.
    //
    // Both paths emit the same primitives, so a collider keeps its colour and shape when play starts.
    // The view is drawn from the simulated pose, not the interpolated one rendering uses, so a fast
    // body's wireframe can lag its mesh by up to one fixed step - which is the pose it really has.
    class PhysicsDebugOverlay final : public PhysicsDebugSink
    {
    public:
        PhysicsDebugOverlay() = default;

        // Draws into debugDraw, which the caller has not begun yet. Requires a current GL context,
        // because the target is a GPU line renderer; a caller without one draws nothing.
        void Draw(EditorContext& context, DebugDraw& debugDraw, const Math::Mat4& viewProjection);

        // -------------------------------------------------------------- PhysicsDebugSink
        void Segment(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color) override;
        void Point(const Math::Vec3& position, float extent, const Math::Vec4& color) override;
        void Sphere(const Math::Vec3& center, float radius, const Math::Vec4& color) override;
        void Capsule(const Math::Vec3& start, const Math::Vec3& end, float radius, const Math::Vec4& color) override;
        void Box(const Math::Vec3& halfExtents, const Math::Vec3& center, const Math::Quat& rotation,
                 const Math::Vec4& color) override;
        void Bounds(const Math::Vec3& minimum, const Math::Vec3& maximum, const Math::Vec4& color) override;

    private:
        // The authored colliders of a scene that is not simulating.
        void DrawAuthoredColliders(Scene& scene);

        // Set for the duration of one Draw call; every sink callback needs it.
        DebugDraw* m_Target = nullptr;
    };
}
