// Lua module loader tests.
//
// The sandboxed `require` reads its modules through the virtual file system, so a case can write
// real .lua files into a temporary directory (or serve them from the shared test package) and check
// what the loader does with them. Lua itself needs no window and no GPU resource, which is what
// keeps the whole suite headless.

#include <doctest/doctest.h>

#include "Scripting/Lua/LuaScriptRequire.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <filesystem>
#include <string>

using namespace HachimiEngine;

namespace
{
    // One Lua state with the same sandbox the runtime opens: core language libraries, and the
    // engine's require as the only module loader.
    class LuaRequireHarness
    {
    public:
        explicit LuaRequireHarness(std::filesystem::path scriptRoot)
            : Root(std::move(scriptRoot))
        {
            State.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
            RegisterLuaRequire(State, Root, Cache);
        }

        void Write(const std::filesystem::path& relativePath, const std::string& source) const
        {
            const std::filesystem::path path = Root / relativePath;
            FileSystem::CreateDirectories(path.parent_path());
            REQUIRE(FileSystem::WriteTextFile(path, source));
        }

        sol::protected_function_result Run(const std::string& code)
        {
            return State.safe_script(code, sol::script_pass_on_error, "require test");
        }

        static std::string ErrorOf(const sol::protected_function_result& result)
        {
            const sol::error error = result;
            return error.what();
        }

        // Declaration order matters: everything holding a Lua value is declared after the state, so
        // it is destroyed while the state is still open.
        sol::state State;
        LuaModuleCache Cache;
        std::filesystem::path Root;
    };
}

TEST_SUITE("Scripting")
{
    TEST_CASE("a module is executed once and every require returns the same table")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireCache"));
        harness.Write("Counter.lua",
            "runs = (runs or 0) + 1\n"
            "return { runs = runs }\n");

        const sol::protected_function_result result = harness.Run(
            "local first = require('Counter')\n"
            "local second = require('Counter')\n"
            "local third = require('./Counter')\n"
            "same = (first == second) and (second == third)\n"
            "runs = first.runs\n");

        REQUIRE(result.valid());
        CHECK(harness.State["same"].get<bool>());
        // The file body ran once even though it was required three times.
        CHECK(harness.State["runs"].get<int>() == 1);
    }

    TEST_CASE("the extension is optional and subdirectories are paths")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequirePaths"));
        harness.Write("Plain.lua", "return { value = 1 }\n");
        harness.Write("sub/Util.lua", "return { value = 2 }\n");
        harness.Write("sub\\Backslash.lua", "return { value = 3 }\n");

        const sol::protected_function_result result = harness.Run(
            "plain = require('Plain').value\n"
            "named = require('Plain.lua').value\n"
            "nested = require('sub/Util').value\n"
            "windows = require('sub\\\\Backslash').value\n");

        REQUIRE(result.valid());
        CHECK(harness.State["plain"].get<int>() == 1);
        CHECK(harness.State["named"].get<int>() == 1);
        CHECK(harness.State["nested"].get<int>() == 2);
        CHECK(harness.State["windows"].get<int>() == 3);
    }

    TEST_CASE("a module that returns nothing still counts as loaded")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireGlobals"));
        harness.Write("SideEffect.lua", "created = true\n");

        const sol::protected_function_result result = harness.Run("value = require('SideEffect')");

        REQUIRE(result.valid());
        // Lua's own loader answers true for a module with no return value, and a module body runs
        // against the global table just like it would there.
        CHECK(harness.State["value"].get<bool>());
        CHECK(harness.State["created"].get<bool>());
    }

    TEST_CASE("a path that leaves Assets/Scripts is refused and never read")
    {
        const std::filesystem::path root = Tests::Workspace().PrepareDirectory("ScriptRequireEscape");
        REQUIRE(FileSystem::WriteTextFile(root.parent_path() / "Outside.lua", "escaped = true\n"));

        LuaRequireHarness harness(root);
        const sol::protected_function_result result = harness.Run(
            "local ok = pcall(function() require('../Outside') end)\n"
            "refused = not ok\n"
            "escapedWasSet = (escaped ~= nil)\n");

        REQUIRE(result.valid());
        // The refusal is a Lua error, so a script can catch it, and the file next to the script root
        // - which does exist and can be read - was still never executed.
        CHECK(harness.State["refused"].get<bool>());
        CHECK_FALSE(harness.State["escapedWasSet"].get<bool>());
    }

    TEST_CASE("an absolute or drive-qualified path is refused")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireAbsolute"));

        CHECK_FALSE(harness.Run("require('C:/Windows/System32/Absolute')").valid());
        CHECK_FALSE(harness.Run("require('/Absolute')").valid());
        CHECK_FALSE(harness.Run("require('C:Relative')").valid());
        CHECK_FALSE(harness.Run("require('\\\\Absolute')").valid());
    }

    TEST_CASE("a name that is not a Lua module is refused")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireExtension"));
        harness.Write("Data.txt", "return { value = 1 }\n");

        // Only .lua is looked for: appending ".lua" to a name that already claims another kind of
        // file would search for a file nobody wrote.
        CHECK_FALSE(harness.Run("require('Data.txt')").valid());
        // An empty or non-string name is a refusal too, not a crash.
        CHECK_FALSE(harness.Run("require('')").valid());
        CHECK_FALSE(harness.Run("require(42)").valid());
    }

    TEST_CASE("a missing module reports the name instead of throwing out of Lua")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireMissing"));

        const sol::protected_function_result result = harness.Run("require('Absent')");
        REQUIRE_FALSE(result.valid());

        const std::string error = LuaRequireHarness::ErrorOf(result);
        CHECK(error.find("Absent") != std::string::npos);
    }

    TEST_CASE("a syntax error in a module keeps its file and line")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireSyntax"));
        harness.Write("Broken.lua", "local value = 1\n"
                                    "this is not lua\n");

        const sol::protected_function_result result = harness.Run("require('Broken')");
        REQUIRE_FALSE(result.valid());

        const std::string error = LuaRequireHarness::ErrorOf(result);
        CHECK(error.find("Broken.lua:2") != std::string::npos);
    }

    TEST_CASE("a circular require stops with an explanation")
    {
        LuaRequireHarness harness(Tests::Workspace().PrepareDirectory("ScriptRequireCycle"));
        harness.Write("First.lua", "require('Second')\nreturn {}\n");
        harness.Write("Second.lua", "require('First')\nreturn {}\n");

        const sol::protected_function_result result = harness.Run("require('First')");
        REQUIRE_FALSE(result.valid());

        const std::string error = LuaRequireHarness::ErrorOf(result);
        CHECK(error.find("circular") != std::string::npos);
    }

    TEST_CASE("a module inside a package loads through the virtual file system")
    {
        Tests::ScopedArchiveMount mount;
        REQUIRE(mount.IsMounted());

        // The package's script directory lives at the mount point and has no loose copy next to it,
        // so a module that loads here can only have come out of the .hpak.
        LuaRequireHarness harness(Tests::Workspace().Root() / "Assets" / "Scripts");

        VirtualFileSystem::ResetStatistics();
        const sol::protected_function_result result = harness.Run(
            "local module = require('script_0')\n"
            "loaded = (type(module) == 'table') and (type(module.OnCreate) == 'function')\n");

        REQUIRE(result.valid());
        CHECK(harness.State["loaded"].get<bool>());
        CHECK(VirtualFileSystem::GetStatistics().ArchiveReadCount > 0);
    }
}
