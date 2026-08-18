#pragma once

#include "Scripting/ScriptEngine.h"

namespace HachimiEngine
{
    class Scene;
    class ScriptRuntime;

    // Lua 5.4 backend. Registered by ScriptManager::Init.
    class LuaScriptEngine final : public ScriptEngine
    {
    public:
        std::string_view GetName() const override;
        const std::vector<std::string>& GetExtensions() const override;
        bool SupportsFile(const std::string& filePath) const override;
        Scope<ScriptRuntime> CreateRuntime(Scene& scene) const override;
    };
}
