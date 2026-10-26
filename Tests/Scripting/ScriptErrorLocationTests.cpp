// Script error location tests.
//
// A backend reports a failure as the text Lua produced; turning that text into a file, a line and a
// message is a pure string operation, which is why it is testable without a VM, a window or a
// project. The cases below are the shapes Lua and sol2 actually produce.

#include <doctest/doctest.h>

#include "Scripting/ScriptErrorLocation.h"

#include <string>

using namespace HachimiEngine;

TEST_SUITE("Scripting")
{
    TEST_CASE("a plain path prefix becomes a file and a line")
    {
        const ScriptLocation location = ParseScriptError(
            "Assets/Scripts/Player.lua:12: attempt to index a nil value (global 'speed')");

        CHECK(location.File == "Assets/Scripts/Player.lua");
        CHECK(location.Line == 12);
        CHECK(location.Message == "attempt to index a nil value (global 'speed')");
    }

    TEST_CASE("a Windows path keeps its own colon")
    {
        const ScriptLocation location = ParseScriptError(
            "C:\\Projects\\Game\\Assets\\Scripts\\Player.lua:7: bad argument");

        // Splitting on the first colon would cut the drive letter off and read the path as the
        // location's text.
        CHECK(location.File == "C:\\Projects\\Game\\Assets\\Scripts\\Player.lua");
        CHECK(location.Line == 7);
        CHECK(location.Message == "bad argument");
    }

    TEST_CASE("a loader chunk name loses its wrapper")
    {
        const ScriptLocation location = ParseScriptError("[string \"Player.lua\"]:3: unexpected symbol");

        CHECK(location.File == "Player.lua");
        CHECK(location.Line == 3);
        CHECK(location.Message == "unexpected symbol");
    }

    TEST_CASE("a traceback is dropped after the first line")
    {
        const ScriptLocation location = ParseScriptError(
            "Assets/Scripts/Player.lua:12: attempt to call a nil value\n"
            "stack traceback:\n"
            "\t[C]: in function 'update'\n"
            "\tAssets/Scripts/Player.lua:12: in function 'OnUpdate'\n");

        CHECK(location.File == "Assets/Scripts/Player.lua");
        CHECK(location.Line == 12);
        CHECK(location.Message == "attempt to call a nil value");
    }

    TEST_CASE("a message without a location keeps the fallback file")
    {
        const ScriptLocation location = ParseScriptError("not enough memory", "Player.lua");

        CHECK(location.File == "Player.lua");
        CHECK(location.Line == 0);
        CHECK(location.Message == "not enough memory");
    }

    TEST_CASE("a colon without a line number is part of the message")
    {
        const ScriptLocation location = ParseScriptError("assertion failed: value is nil", "Player.lua");

        CHECK(location.File == "Player.lua");
        CHECK(location.Line == 0);
        CHECK(location.Message == "assertion failed: value is nil");
    }

    TEST_CASE("line numbers with more than one digit are read whole")
    {
        const ScriptLocation location = ParseScriptError("Player.lua:1234: boom");
        CHECK(location.Line == 1234);
        CHECK(location.Message == "boom");
    }

    TEST_CASE("an empty or whitespace-only message does not crash")
    {
        const ScriptLocation empty = ParseScriptError("", "Player.lua");
        CHECK(empty.File == "Player.lua");
        CHECK(empty.Line == 0);
        CHECK(empty.Message.empty());

        const ScriptLocation blank = ParseScriptError("   \n\tsecond line", "Player.lua");
        CHECK(blank.Line == 0);
        CHECK(blank.Message.empty());
    }

    TEST_CASE("formatting puts the location in front of the message")
    {
        ScriptLocation location;
        location.File = "Player.lua";
        location.Line = 12;
        location.Message = "attempt to index a nil value";
        CHECK(FormatScriptLocation(location) == "Player.lua:12: attempt to index a nil value");

        // A file without a line is still worth printing: it says which script failed.
        location.Line = 0;
        CHECK(FormatScriptLocation(location) == "Player.lua: attempt to index a nil value");

        ScriptLocation bare;
        bare.Message = "not enough memory";
        CHECK(FormatScriptLocation(bare) == "not enough memory");
    }
}
