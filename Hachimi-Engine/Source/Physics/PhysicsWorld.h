#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Physics/PhysicsEvents.h"
#include "Physics/PhysicsQuery.h"
#include "Physics/PhysicsSettings.h"
#include "Physics/PhysicsTaskScheduler.h"
#include "Renderer/PhysicsDebugDraw.h"
#include "Math/Math.h"

#include <cstdint>
#include <vector>

namespace HachimiEngine
{
    class Scene;

    // Wraps a Box3D world and keeps the EnTT entity -> b3BodyId mapping.
    // Box3D is a C17 library, so its types are hidden behind an Impl to keep
    // client/editor code independent from the third-party headers.
    class PhysicsWorld
    {
    public:
        explicit PhysicsWorld(const PhysicsSettings& settings);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Creates Box3D bodies for every entity that currently has a
        // RigidbodyComponent, a ColliderComponent, and a TransformComponent.
        void CreateBodies(Scene& scene);

        // Synchronizes ECS changes with Box3D, advances the simulation by exactly one fixed step,
        // writes dynamic body transforms back to the ECS, and refills GetEvents() with what that step
        // produced.
        //
        // One call is one b3World_Step. The accumulator that decides how many steps a frame owes
        // belongs to Scene, so this must be called once per FixedUpdate phase - and never from a
        // JobSystem task, which would deadlock inside Box3D. A step asked for from a pool worker is
        // refused with an error rather than attempted; see PhysicsTaskScheduler.
        void Update(Scene& scene, Timestep fixedTimestep);

        // How the last steps used the engine's thread pool, plus the worker count Box3D was given.
        // Read by the editor's physics panel and by the tests that pin the threading rules.
        PhysicsTaskStats GetTaskStats() const;

        // ------------------------------------------------------- debug view
        //
        // Reports what the simulation holds - collider geometry with the body's current pose, plus
        // the broad phase and contact diagnostics the options ask for - as primitives the caller
        // draws. The backend's own callbacks and the shape records it keeps between frames stay
        // inside this class, so a caller sees only engine math types.
        //
        // Read-only and safe on an empty world: a world with no bodies reports nothing. Nothing here
        // creates a GPU resource, so a test can capture the output without a window.
        void DrawDebug(PhysicsDebugSink& sink, const PhysicsDebugDrawOptions& options = {}) const;

        // Destroys the body associated with an entity, if one exists.
        void DestroyBody(uint64_t entityKey);

        bool IsRunning() const;

        // Whether an entity currently has a simulated body.
        bool HasBody(uint64_t entityKey) const;

        // ---------------------------------------------------------------- events
        //
        // What the last Update produced. The queue is refilled by every step and therefore only
        // describes the step that just ran; a caller that wants events must read them before the next
        // Update. PhysicsSystem does exactly that, once per fixed step, and turns them into scene
        // events - the world itself never dispatches, so events can be inspected in a test without a
        // dispatcher and without a subscriber.
        const PhysicsEventQueue& GetEvents() const;

        // ---------------------------------------------------------------- queries
        //
        // Queries never create an Entity: they report the owning entity's key, and the caller
        // resolves it through Scene::GetEntityByKey. That keeps the physics layer free of ECS
        // handle semantics and lets a query be answered without a Scene at all.

        // Closest shape along the ray. Direction need not be normalized; Distance is measured in
        // world units either way. Returns false and leaves outResult untouched when nothing is hit.
        bool RayCast(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            RayCastResult& outResult,
            const PhysicsQueryFilter& filter = {}) const;

        // Every shape along the ray, nearest first. Appends to outResults and stops at maxResults,
        // so the caller controls the cost. Returns the number of hits appended.
        size_t RayCastAll(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            RayCastResults& outResults,
            size_t maxResults = DefaultMaxQueryResults,
            const PhysicsQueryFilter& filter = {}) const;

        // Shapes overlapping a sphere. Appends to outResults and returns how many were appended.
        size_t OverlapSphere(
            const Math::Vec3& center,
            float radius,
            OverlapResults& outResults,
            size_t maxResults = DefaultMaxQueryResults,
            const PhysicsQueryFilter& filter = {}) const;

        // Shapes overlapping a box, given its half extents in its own space and its rotation.
        size_t OverlapBox(
            const Math::Vec3& center,
            const Math::Vec3& halfExtents,
            const Math::Quat& rotation,
            OverlapResults& outResults,
            size_t maxResults = DefaultMaxQueryResults,
            const PhysicsQueryFilter& filter = {}) const;

        // Sweeps a sphere along translation and reports the first thing it would hit. Returns false
        // when the sweep is clear for its whole length.
        bool ShapeCastSphere(
            const Math::Vec3& origin,
            const Math::Vec3& translation,
            float radius,
            ShapeCastResult& outResult,
            const PhysicsQueryFilter& filter = {}) const;

        // ------------------------------------------------------- motion state
        //
        // Every accessor below reports no body the same way: a getter returns zero and leaves its
        // out parameter alone, a setter does nothing. Callers therefore need no "does this entity
        // have a body" branch around every call, which is what makes these usable from a script.

        Math::Vec3 GetLinearVelocity(uint64_t entityKey) const;
        void SetLinearVelocity(uint64_t entityKey, const Math::Vec3& velocity);
        Math::Vec3 GetAngularVelocity(uint64_t entityKey) const;
        void SetAngularVelocity(uint64_t entityKey, const Math::Vec3& velocity);

        // Continuous force, accumulated until the next step. Force applies at the centre of mass.
        void ApplyForce(uint64_t entityKey, const Math::Vec3& force);
        // Continuous force applied at a world point, which also produces a torque.
        void ApplyForceAtPoint(uint64_t entityKey, const Math::Vec3& force, const Math::Vec3& worldPoint);
        // Instantaneous change of momentum, applied at the centre of mass.
        void ApplyImpulse(uint64_t entityKey, const Math::Vec3& impulse);
        void ApplyImpulseAtPoint(uint64_t entityKey, const Math::Vec3& impulse, const Math::Vec3& worldPoint);
        void ApplyTorque(uint64_t entityKey, const Math::Vec3& torque);
        void ApplyAngularImpulse(uint64_t entityKey, const Math::Vec3& impulse);

        // Total mass of the body's shapes. Zero for a static or kinematic body.
        float GetMass(uint64_t entityKey) const;

        bool IsAwake(uint64_t entityKey) const;
        void SetAwake(uint64_t entityKey, bool awake);

        // Moves an entity's body to a new world transform without simulating the move, and clears
        // the interpolation history so the entity is not drawn halfway between the two positions.
        // The scene transform is written too, through the same parent chain the physics write-back
        // uses, so a teleported entity does not snap back on the next write-back.
        void Teleport(uint64_t entityKey, const Math::Vec3& position, const Math::Quat& rotation, Scene& scene);

    private:
        struct Impl;

        Scope<Impl> m_Impl;
    };
}
