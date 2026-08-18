#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    class ScriptEngine;

    // Registry of scripting language backends, keyed by file extension.
    // The registry is engine-wide; each running scene gets separate ScriptRuntime objects.
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

        // Script asset root: <Project>/Assets/Scripts.
        static std::filesystem::path GetScriptsDirectory();

        // Converts a serialized script path (relative to Assets/Scripts) to an absolute path.
        static std::filesystem::path ResolveScriptPath(const std::string& relativePath);

    private:
        static std::vector<Scope<ScriptEngine>> s_Engines;
    };
}
