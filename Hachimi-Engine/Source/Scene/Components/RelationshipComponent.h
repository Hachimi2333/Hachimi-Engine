#pragma once

#include "Core/UUID.h"
#include "Scene/ComponentRegistry.h"

#include <vector>

namespace HachimiEngine
{
    // Where an entity sits in the scene hierarchy.
    //
    // Only the parent is stored. Children are derived by Scene, so the two directions cannot
    // disagree: the previous mirrored Children list was written by nobody, which is why
    // destroying a parent used to orphan its children.
    struct RelationshipComponent
    {
        UUID Parent = UUID::Invalid();
    };

    ComponentDescriptor MakeRelationshipComponentDescriptor();
}
