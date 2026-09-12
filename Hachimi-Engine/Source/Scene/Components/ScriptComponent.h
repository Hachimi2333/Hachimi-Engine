#pragma once

#include "Scene/ComponentRegistry.h"

#include <string>
#include <vector>

namespace HachimiEngine
{
    struct ScriptComponent
    {
        struct ScriptReference
        {
            // Path relative to the project Assets/Scripts directory, for example
            // "Rotator.lua" or "Player/Controller.lua". The backend language is
            // resolved from the file extension by ScriptManager.
            std::string Path;
            bool Enabled = true;
        };

        std::vector<ScriptReference> Scripts;
    };

    ComponentDescriptor MakeScriptComponentDescriptor();
}
