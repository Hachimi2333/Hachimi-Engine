#pragma once

#include "Asset/AssetHandle.h"
#include "Scene/ComponentRegistry.h"

#include <string>
#include <vector>

namespace HachimiEngine
{
    struct ScriptComponent
    {
        struct ScriptReference
        {
            // Script asset under Assets/Scripts. The handle is the reference; renaming or moving
            // the .lua file keeps it working.
            AssetHandle Script;

            // File name without extension, kept in the scene so a reference whose asset is missing
            // still says what it used to point at, and so the inspector can label it without a
            // database lookup. Never used to resolve the file.
            std::string DisplayName;

            bool Enabled = true;
        };

        std::vector<ScriptReference> Scripts;
    };

    ComponentDescriptor MakeScriptComponentDescriptor();
}
