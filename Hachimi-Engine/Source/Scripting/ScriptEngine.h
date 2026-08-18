#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <string>
#include <string_view>
#include <vector>

namespace HachimiEngine
{
    class Scene;
    class ScriptRuntime;

    // Language backend factory. Each scripting language supported by the engine
    // implements this interface and registers itself with ScriptManager.
    class ScriptEngine
    {
    public:
        virtual ~ScriptEngine() = default;

        // Stable display name used for logging and diagnostics, e.g. "Lua".
        virtual std::string_view GetName() const = 0;

        // Lowercase file extensions handled by this backend, e.g. { ".lua" }.
        virtual const std::vector<std::string>& GetExtensions() const = 0;

        // Returns true when the file path matches one of this backend's extensions.
        virtual bool SupportsFile(const std::string& filePath) const = 0;

        // Creates one isolated execution context for a running scene.
        virtual Scope<ScriptRuntime> CreateRuntime(Scene& scene) const = 0;
    };
}
