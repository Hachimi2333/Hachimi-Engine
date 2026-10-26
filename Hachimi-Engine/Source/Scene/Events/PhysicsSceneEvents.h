#pragma once

#include "Physics/PhysicsEvents.h"

#include <vector>

namespace HachimiEngine
{
    // What one simulation step reported, in the form the scene hands to its subscribers.
    //
    // The physics world reports one struct per touch, which is what a handler wants but is expensive
    // to publish: a busier step would mean one signal call per contact pair. These bundle a step's
    // worth of one kind into a single event, so a subscriber is called once per step per kind and
    // iterates what it cares about. The individual records are the same structs the physics layer
    // produces - nothing is re-derived here, so a subscriber sees the point, the normal and the
    // approach speed exactly as Box3D reported them.
    //
    // Scene events travel through Scene::GetDispatcher and are triggered with an immediate call,
    // before the update phase: a handler that runs from here is still inside the simulation step it
    // is reacting to, and by the time scripts get their Update the whole batch has been delivered.
    // That ordering is the guarantee worth relying on - "an event says what the last step did, and
    // nothing has moved since".

    // Solid colliders that started or stopped touching during the step.
    struct SceneCollisionEvent
    {
        std::vector<CollisionBeginEvent> Begins;
        std::vector<CollisionEndEvent> Ends;
        std::vector<CollisionHitEvent> Hits;

        bool IsEmpty() const { return Begins.empty() && Ends.empty() && Hits.empty(); }
    };

    // Colliders that entered or left a trigger volume during the step.
    //
    // Both lists describe the same step, and Box3D reports a pair as beginning or ending but never
    // both at once, so an entity cannot appear on both sides of one event.
    struct SceneTriggerEvent
    {
        std::vector<TriggerBeginEvent> Begins;
        std::vector<TriggerEndEvent> Ends;

        bool IsEmpty() const { return Begins.empty() && Ends.empty(); }
    };
}
