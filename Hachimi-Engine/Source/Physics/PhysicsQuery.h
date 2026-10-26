#pragma once

#include "Math/Math.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace HachimiEngine
{
    inline constexpr uint64_t InvalidPhysicsEntityKey = std::numeric_limits<uint64_t>::max();

    // One hit from a ray cast.
    //
    // EntityKey is the registry handle, including the valid handle zero. Only successful queries
    // produce a result; ownerless or destroyed shapes are filtered before publication. The physics layer deliberately stops there: it never creates or holds an
    // Entity, so a result cannot outlive the entity it names and callers resolve the key through
    // Scene::GetEntityByKey.
    struct RayCastResult
    {
        uint64_t EntityKey = 0;
        Math::Vec3 Point { 0.0f };
        Math::Vec3 Normal { 0.0f };
        // Distance from the origin along the ray, in world units.
        float Distance = 0.0f;
    };

    // One shape found by an overlap query. Overlap queries report no contact point: Box3D answers
    // which shapes overlap, and a penetrating box has no single point that describes the overlap.
    struct OverlapResult
    {
        uint64_t EntityKey = 0;
        // World-space centre of the overlapping shape's body.
        Math::Vec3 Position { 0.0f };
    };

    // One hit from a shape cast.
    struct ShapeCastResult
    {
        uint64_t EntityKey = 0;
        Math::Vec3 Point { 0.0f };
        Math::Vec3 Normal { 0.0f };
        // How far the shape travelled before it hit.
        float Distance = 0.0f;
    };

    // Which shapes a query may hit, and which entities it must ignore.
    //
    // The bits work the way collider bits do: a shape is a candidate when
    // (shape.CategoryBits & MaskBits) != 0 and (CategoryBits & shape.MaskBits) != 0. The defaults
    // accept everything, which is what a query with no filter means.
    //
    // ExcludeEntityKey is the separate axis, and it exists because the bits cannot express "not the
    // entity that fired this ray": a self-query has to be excluded by identity. A query that must
    // ignore more than one entity - a character ignoring several carried objects - still needs the
    // bits, so the two mechanisms are kept apart rather than overloading one.
    struct PhysicsQueryFilter
    {
        uint64_t CategoryBits = ~0ull;
        uint64_t MaskBits = ~0ull;
        uint64_t ExcludeEntityKey = InvalidPhysicsEntityKey;
    };

    // Default cap on how many hits RayCastAll and the overlap queries return. A query from a script
    // runs inside a frame, so an unbounded result list is a way to lose the frame, not a feature.
    inline constexpr size_t DefaultMaxQueryResults = 32;

    using RayCastResults = std::vector<RayCastResult>;
    using OverlapResults = std::vector<OverlapResult>;
}
