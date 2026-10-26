#pragma once

#include <sol/sol.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // What a runtime's `require` remembers between calls.
    //
    // It holds Lua values, so it has to be destroyed while its state is still alive: declare it
    // after the sol::state it belongs to, and clear it from the runtime's shutdown.
    struct LuaModuleCache
    {
        // Keyed by the resolved path, so two names that reach the same file share one table.
        std::unordered_map<std::string, sol::object> Modules;
        std::unordered_map<std::string, std::filesystem::file_time_type> WriteTimes;
        // Modules currently executing, innermost last. Without this a module that requires itself,
        // directly or through a cycle, would recurse until the Lua stack ran out.
        std::vector<std::string> Loading;

        void Clear()
        {
            Loading.clear();
            Modules.clear();
            WriteTimes.clear();
        }
    };

    // Installs the sandboxed `require` a script module loads other modules with.
    //
    // Lua's own package library is not opened, so this is the only loader a script can reach, and
    // it is deliberately narrower than Lua's:
    //   - a name is a path relative to scriptRoot (the project's Assets/Scripts), not a dotted
    //     module name;
    //   - ".lua" is appended when the name has no extension, and any other extension is refused;
    //   - the normalized path must stay inside scriptRoot, so an absolute path, a drive letter or
    //     a ".." segment is reported and raised as a Lua error instead of being read;
    //   - every module is executed once and its result cached per runtime, so a second require of
    //     the same file returns the same table;
    //   - sources are read through the virtual file system, which is what makes a packaged project
    //     load the same module out of its .hpak as the editor loads from disk.
    void RegisterLuaRequire(sol::state& state, const std::filesystem::path& scriptRoot, LuaModuleCache& cache);
}
