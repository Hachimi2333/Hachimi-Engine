#pragma once

#include "Core/Base.h"

#include <string>
#include <string_view>

namespace HachimiEngine
{
    // Where a script error happened, as far as the backend could tell.
    struct ScriptLocation
    {
        // Path, chunk name or display name of the script. Empty only when the message carried no
        // location and the caller had no fallback to offer.
        std::string File;
        // 1-based line, 0 when the message did not say.
        int Line = 0;
        // The error text with the location prefix removed and a trailing traceback dropped.
        std::string Message;
    };

    // Parses the "file:line: message" prefix a Lua error carries: the plain path form
    // ("Assets/Scripts/Player.lua:12: attempt to index a nil value") and the loader form
    // ("[string \"Player.lua\"]:12: ...") both end up as a file and a line.
    //
    // A Windows path contains a colon of its own, so the prefix is found by looking for a colon
    // followed by digits, not by splitting on the first colon. Everything after the first line is
    // dropped, because Lua appends a traceback that would turn one console entry into a screenful.
    ScriptLocation ParseScriptError(std::string_view errorMessage, std::string_view fallbackFile = {});

    // "Player.lua:12: attempt to index a nil value", or the bare message when the message did not
    // say where it happened. This is what a console line starts with.
    std::string FormatScriptLocation(const ScriptLocation& location);
}
