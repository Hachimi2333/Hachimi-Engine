#pragma once

#include "Math/Math.h"

#include <cstdint>
#include <vector>

namespace HachimiEngine
{
    // What happened between two entities, named by entity key exactly like a query result.
    //
    // Events are collected from Box3D after a step and dispatched before the update phase, so a
    // handler always sees "the simulation step that just ran" rather than a half-applied one. The
    // physics layer reports keys, never Entity handles: the entity behind a key may already be gone
    // by the time a handler runs, and a handle would have hidden that.
    //
    // Beginning and ending are separate types rather than one struct with a flag, because the two
    // carry different data: a beginning knows where the surfaces touched, an ending only knows that
    // they stopped. Neither can be filled in for the other - a contact that ended during this step
    // has no manifold left to read a point from - so the type makes that visible instead of leaving a
    // zeroed point for the caller to interpret.

    // Two solid colliders began touching.
    struct CollisionBeginEvent
    {
        uint64_t EntityKeyA = 0;
        uint64_t EntityKeyB = 0;
        // World-space point between the two surfaces, taken from the contact record of the step that
        // began the touch.
        //
        // Best effort, and worth knowing about before relying on it: the manifold a begin event points
        // at is the one the narrow phase produced for that step, so a contact that starts while the
        // two bodies are still being pushed apart can report a point that lags the surfaces by a
        // fraction of a step. The entity pair and the normal are exact - they are what a listener
        // reacts to - and a game that needs a precise hit position wants CollisionHitEvent, which
        // Box3D fills in with the impact the solver actually resolved.
        Math::Vec3 Point { 0.0f };
        // Unit normal pointing from A to B.
        Math::Vec3 Normal { 0.0f };
        float Impulse = 0.0f;
    };

    // Two solid colliders stopped touching. Reported with keys alone: the contact is gone.
    struct CollisionEndEvent
    {
        uint64_t EntityKeyA = 0;
        uint64_t EntityKeyB = 0;
    };

    // Two colliders met with an approach speed above Box3D's hit threshold. This is the event a
    // gameplay listener uses for "how hard was that", so it carries the contact data a begin event
    // does not: the point, the normal and the approach speed.
    struct CollisionHitEvent
    {
        uint64_t EntityKeyA = 0;
        uint64_t EntityKeyB = 0;
        Math::Vec3 Point { 0.0f };
        // Unit normal pointing from A to B.
        Math::Vec3 Normal { 0.0f };
        // Closing speed along the normal, in world units per second. Always positive.
        float ApproachSpeed = 0.0f;
    };

    // A collider entered a trigger volume. Sensor is the entity carrying the IsTrigger collider,
    // Other is what walked into it; a trigger volume reports what it contains, so the sensor is the
    // side worth naming first.
    struct TriggerBeginEvent
    {
        uint64_t SensorEntityKey = 0;
        uint64_t OtherEntityKey = 0;
    };

    // A collider left a trigger volume.
    struct TriggerEndEvent
    {
        uint64_t SensorEntityKey = 0;
        uint64_t OtherEntityKey = 0;
    };

    // Everything one simulation step produced, filled by PhysicsWorld::Update and read once.
    //
    // Collected into arrays rather than handed out through callbacks: Box3D steps multi-threaded and
    // its event arrays are only safe to read once the step is over. The queue is the place where
    // "events that arrived during the step" becomes "events a game can look at", which is also where
    // each one is checked against the live body table so a destroyed entity is dropped rather than
    // reported with a stale key.
    //
    // Filled in place instead of being created per step, so a steady-state scene does not allocate.
    class PhysicsEventQueue
    {
    public:
        void Clear()
        {
            m_CollisionBegin.clear();
            m_CollisionEnd.clear();
            m_CollisionHit.clear();
            m_TriggerBegin.clear();
            m_TriggerEnd.clear();
        }

        bool IsEmpty() const
        {
            return m_CollisionBegin.empty() && m_CollisionEnd.empty() && m_CollisionHit.empty()
                && m_TriggerBegin.empty() && m_TriggerEnd.empty();
        }

        // How many events of every kind are waiting. Used by diagnostics and by the tests that pin
        // "one step, one event".
        size_t GetCount() const
        {
            return m_CollisionBegin.size() + m_CollisionEnd.size() + m_CollisionHit.size()
                + m_TriggerBegin.size() + m_TriggerEnd.size();
        }

        const std::vector<CollisionBeginEvent>& GetCollisionBegin() const { return m_CollisionBegin; }
        const std::vector<CollisionEndEvent>& GetCollisionEnd() const { return m_CollisionEnd; }
        const std::vector<CollisionHitEvent>& GetCollisionHit() const { return m_CollisionHit; }
        const std::vector<TriggerBeginEvent>& GetTriggerBegin() const { return m_TriggerBegin; }
        const std::vector<TriggerEndEvent>& GetTriggerEnd() const { return m_TriggerEnd; }

        void PushCollisionBegin(const CollisionBeginEvent& event) { m_CollisionBegin.push_back(event); }
        void PushCollisionEnd(const CollisionEndEvent& event) { m_CollisionEnd.push_back(event); }
        void PushCollisionHit(const CollisionHitEvent& event) { m_CollisionHit.push_back(event); }
        void PushTriggerBegin(const TriggerBeginEvent& event) { m_TriggerBegin.push_back(event); }
        void PushTriggerEnd(const TriggerEndEvent& event) { m_TriggerEnd.push_back(event); }

    private:
        std::vector<CollisionBeginEvent> m_CollisionBegin;
        std::vector<CollisionEndEvent> m_CollisionEnd;
        std::vector<CollisionHitEvent> m_CollisionHit;
        std::vector<TriggerBeginEvent> m_TriggerBegin;
        std::vector<TriggerEndEvent> m_TriggerEnd;
    };
}
