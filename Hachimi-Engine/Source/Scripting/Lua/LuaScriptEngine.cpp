#include "Scripting/Lua/LuaScriptEngine.h"

#include "Scripting/Lua/LuaScriptRuntime.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace HachimiEngine
{
    std::string_view LuaScriptEngine::GetName() const
    {
        return "Lua";
    }

    const std::vector<std::string>& LuaScriptEngine::GetExtensions() const
    {
        static const std::vector<std::string> extensions = { ".lua" };
        return extensions;
    }

    bool LuaScriptEngine::SupportsFile(const std::string& filePath) const
    {
        std::string extension = std::filesystem::path(filePath).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

        return extension == ".lua";
    }

    Scope<ScriptRuntime> LuaScriptEngine::CreateRuntime(Scene& scene) const
    {
        return CreateScope<LuaScriptRuntime>(scene);
    }
}
