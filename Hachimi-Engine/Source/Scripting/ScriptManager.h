#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <string>
#include <vector>

namespace HachimiEngine
{
    class ScriptEngine;

    // Registry of scripting language backends, keyed by file extension.
    // The registry is engine-wide; each running scene gets separate ScriptRuntime objects.
    //
    // Asset paths are not resolved here: the asset database owns the mapping from a script
    // reference to its file, and ScriptWorld looks a script up before asking for a backend.
    class ScriptManager
    {
    public:
        static void Init();
        static void Shutdown();

        // Takes ownership of an engine. ScriptManager::Init registers the built-in Lua backend.
        static void RegisterEngine(Scope<ScriptEngine> engine);

        static ScriptEngine* GetEngineForFile(const std::string& filePath);
        static bool IsScriptFile(const std::string& filePath);

        static const std::vector<Scope<ScriptEngine>>& GetEngines() { return s_Engines; }

    private:
        static std::vector<Scope<ScriptEngine>> s_Engines;
    };
}
