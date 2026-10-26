#pragma once

#include <cstdint>

namespace HachimiEngine
{
    // How a Box3D body names the entity that owns it.
    //
    // The key travels in the body's user data, which is what lets a query result, a contact event or
    // a body move event name its entity without a lookup table. Box3D hands that pointer back for one
    // more step after a body is destroyed, so the encoding has to make "no owner" a value of its own:
    // entityKey + 1, because 0 is what every unprepared body and shape starts with. A destroyed body
    // additionally has its user data cleared, so a stale shape reads as 0 instead of as whatever
    // entity inherits the recycled slot next.
    //
    // Kept in a header of its own because two layers need it and neither should own it: PhysicsWorld
    // writes it when it registers a body, the event collector reads it when it decodes an event.
    inline void* EncodePhysicsBodyKey(uint64_t entityKey)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(entityKey + 1));
    }

    inline uint64_t DecodePhysicsBodyKey(const void* userData)
    {
        const uintptr_t value = reinterpret_cast<uintptr_t>(userData);
        return value == 0 ? 0 : static_cast<uint64_t>(value - 1);
    }
}
