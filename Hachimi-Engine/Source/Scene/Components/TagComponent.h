#pragma once

#include "Scene/ComponentRegistry.h"

#include <string>

namespace HachimiEngine
{
    // Human readable entity name shown by the editor.
    struct TagComponent
    {
        std::string Tag = "Entity";
    };

    ComponentDescriptor MakeTagComponentDescriptor();
}
