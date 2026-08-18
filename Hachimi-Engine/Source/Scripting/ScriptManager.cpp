#include "Scripting/ScriptManager.h"

#include "Asset/AssetManager.h"
#include "Scripting/Lua/LuaScriptEngine.h"
#include "Scripting/ScriptEngine.h"

#include <algorithm>

namespace HachimiEngine
{
    std::vector<Scope<ScriptEngine>> ScriptManager::s_Engines;

    void ScriptManager::Init()
    {
        if (!s_Engines.empty())
        {
            return;
        }

        RegisterEngine(CreateScope<LuaScriptEngine>());
    }

    void ScriptManager::Shutdown()
    {
        s_Engines.clear();
    }

    void ScriptManager::RegisterEngine(Scope<ScriptEngine> engine)
    {
        if (engine == nullptr)
        {
            return;
        }

        s_Engines.push_back(std::move(engine));
    }

    ScriptEngine* ScriptManager::GetEngineForFile(const std::string& filePath)
    {
        for (const Scope<ScriptEngine>& engine : s_Engines)
        {
            if (engine->SupportsFile(filePath))
            {
                return engine.get();
            }
        }
        return nullptr;
    }

    bool ScriptManager::IsScriptFile(const std::string& filePath)
    {
        return GetEngineForFile(filePath) != nullptr;
    }

    std::filesystem::path ScriptManager::GetScriptsDirectory()
    {
        return AssetManager::GetAssetsDirectory() / "Scripts";
    }

    std::filesystem::path ScriptManager::ResolveScriptPath(const std::string& relativePath)
    {
        if (relativePath.empty())
        {
            return {};
        }

        return (GetScriptsDirectory() / relativePath).lexically_normal();
    }
}
