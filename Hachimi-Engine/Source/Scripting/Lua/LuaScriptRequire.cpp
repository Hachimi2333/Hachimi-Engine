#include "Scripting/Lua/LuaScriptRequire.h"

#include "Core/Log.h"
#include "Scripting/ScriptErrorLocation.h"
#include "Utils/VirtualFileSystem.h"
#include "Utils/FileSystem.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace HachimiEngine
{
    namespace
    {
        constexpr std::string_view LuaExtension = ".lua";
        // Chunk names are built under this prefix instead of the resolved path. Lua truncates the
        // name it is given to sixty characters, so an absolute path would reach the console with its
        // middle cut out, while "Assets/Scripts/..." is both short and what the author recognizes.
        constexpr std::string_view ChunkNamePrefix = "Assets/Scripts/";

        // Turns a module name into the path relative to scriptRoot it names, or explains why it does
        // not name one. A refused name is a script error rather than a silent miss: a loader that
        // quietly returns nothing is harder to debug than one that stops the script.
        bool ResolveModulePath(const std::string& name, std::filesystem::path& outRelative, std::string& outError)
        {
            if (name.empty())
            {
                outError = "require expects a module name";
                return false;
            }

            std::filesystem::path relative(name);
            if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
            {
                outError = "require('" + name + "') must be a path relative to Assets/Scripts";
                return false;
            }

            if (relative.has_extension())
            {
                // Only Lua modules: appending ".lua" to something that already names another kind
                // of file would look for a file nobody wrote.
                if (relative.extension() != LuaExtension)
                {
                    outError = "require('" + name + "') is not a Lua module; only '.lua' is supported";
                    return false;
                }
            }
            else
            {
                relative += LuaExtension;
            }

            const std::filesystem::path normalized = relative.lexically_normal();
            const auto escapes = std::find(normalized.begin(), normalized.end(), std::filesystem::path(".."));
            if (escapes != normalized.end())
            {
                outError = "require('" + name + "') leaves Assets/Scripts";
                return false;
            }

            outRelative = normalized;
            return true;
        }
    }

    void RegisterLuaRequire(sol::state& state, const std::filesystem::path& scriptRoot, LuaModuleCache& cache)
    {
        // The lambda holds references into the runtime that owns both the state and the cache, and
        // it is only ever called from a script the runtime is running.
        state.set_function("require", [&state, &cache, scriptRoot](const std::string& name) -> sol::object
        {
            std::filesystem::path relative;
            std::string error;
            if (!ResolveModulePath(name, relative, error))
            {
                HE_CORE_ERROR("Lua require failed: {}", error);
                throw std::runtime_error(error);
            }

            const std::filesystem::path modulePath = (scriptRoot / relative).lexically_normal();
            const std::string key = modulePath.generic_string();
            const std::string chunkName = std::string(ChunkNamePrefix) + relative.generic_string();

            if (const auto found = cache.Modules.find(key); found != cache.Modules.end())
            {
                return found->second;
            }

            if (std::find(cache.Loading.begin(), cache.Loading.end(), key) != cache.Loading.end())
            {
                std::string cycle;
                for (const std::string& loading : cache.Loading)
                {
                    cycle += loading;
                    cycle += " -> ";
                }
                cycle += key;

                const std::string message = "require('" + name + "') is circular: " + cycle;
                HE_CORE_ERROR("Lua require failed: {}", message);
                throw std::runtime_error(message);
            }

            if (FileSystem::IsFile(modulePath))
                cache.WriteTimes[key] = FileSystem::GetLastWriteTime(modulePath);
            std::string source;
            if (!VirtualFileSystem::ReadTextFile(modulePath, source))
            {
                const std::string message = "require('" + name + "'): no Lua module at '" + key + "'";
                HE_CORE_ERROR("Lua require failed: {}", message);
                throw std::runtime_error(message);
            }

            cache.Loading.push_back(key);

            // The chunk runs against the global table, exactly like a module Lua's own loader would
            // run: it sees HE and the other modules, and a module that sets a global keeps it.
            const sol::protected_function_result result = state.safe_script(source, sol::script_pass_on_error,
                chunkName);

            cache.Loading.pop_back();

            if (!result.valid())
            {
                const sol::error scriptError = result;
                const std::string message = FormatScriptLocation(ParseScriptError(scriptError.what(), chunkName));
                HE_CORE_ERROR("Failed to load Lua module '{}': {}", name, message);
                throw std::runtime_error(message);
            }

            // Lua's own require gives a module that returns nothing the value true, so a file that
            // only defines globals still counts as loaded.
            sol::object module = result.return_count() > 0 ? result.get<sol::object>(0) : sol::make_object(state, true);
            if (module.get_type() == sol::type::nil)
            {
                module = sol::make_object(state, true);
            }

            cache.Modules.emplace(key, module);
            if (FileSystem::IsFile(modulePath))
                cache.WriteTimes[key] = FileSystem::GetLastWriteTime(modulePath);
            return module;
        });
    }
}
