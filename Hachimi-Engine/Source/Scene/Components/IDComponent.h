#pragma once

#include "Core/UUID.h"
#include "Scene/ComponentRegistry.h"

namespace HachimiEngine
{
    // Stable identity of one entity. Its UUID is what a scene file keys entities by and what a
    // RelationshipComponent points at.
    struct IDComponent
    {
        UUID ID;
    };

    ComponentDescriptor MakeIDComponentDescriptor();
}
