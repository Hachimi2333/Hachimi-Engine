#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Timestep.h"
#include "Scene/Entity.h"

#include <cstdint>
#include <filesystem>

namespace HachimiEngine
{
    class Scene;

    // Execution context for one scripting language inside one running scene.
    // A runtime owns the native VM state and all script instances created through it.
    class ScriptRuntime
    {
    public:
        virtual ~ScriptRuntime() = default;

        // Creates an instance for one script slot. slotIndex identifies the entry
        // inside ScriptComponent::Scripts and is stable for the lifetime of the scene.
        // sourcePath is the resolved on-disk or in-package path of the script asset, which the
        // caller looks up once so a backend never has to know how assets are addressed.
        virtual void CreateInstance(Entity entity, uint32_t slotIndex, const std::filesystem::path& sourcePath,
                                    const std::string& displayName) = 0;

        // Advances every live instance by one frame.
        virtual void Update(Timestep timestep, Scene& scene) = 0;

        // Calls OnDestroy on every live instance, then releases the VM state.
        virtual void Shutdown(Scene& scene) = 0;
    };
}
