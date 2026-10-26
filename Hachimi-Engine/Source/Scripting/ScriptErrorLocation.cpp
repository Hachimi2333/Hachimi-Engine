#include "Scripting/ScriptErrorLocation.h"

#include <cctype>

namespace HachimiEngine
{
    namespace
    {
        bool IsSpace(char character)
        {
            return std::isspace(static_cast<unsigned char>(character)) != 0;
        }

        std::string_view Trim(std::string_view text)
        {
            size_t begin = 0;
            while (begin < text.size() && IsSpace(text[begin]))
            {
                ++begin;
            }

            size_t end = text.size();
            while (end > begin && IsSpace(text[end - 1]))
            {
                --end;
            }

            return text.substr(begin, end - begin);
        }

        // Lua names a chunk "[string \"...\"]" when it was loaded without a name of its own. The
        // wrapper is noise in a console line, so only the name inside it is kept.
        std::string UnwrapChunkName(std::string_view raw)
        {
            if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']')
            {
                return std::string(raw);
            }

            std::string_view inner = raw.substr(1, raw.size() - 2);
            constexpr std::string_view prefix = "string ";
            if (inner.size() > prefix.size() && inner.substr(0, prefix.size()) == prefix)
            {
                inner.remove_prefix(prefix.size());
            }

            if (inner.size() >= 2 && inner.front() == '"' && inner.back() == '"')
            {
                inner = inner.substr(1, inner.size() - 2);
            }

            return std::string(inner);
        }

        // Splits "<file>:<line>: <message>". Returns false when the first line has no location
        // prefix, which is the case for an error raised by the host rather than by a chunk.
        bool SplitLocationPrefix(std::string_view line, size_t& outFileEnd, size_t& outMessageBegin, int& outLine)
        {
            for (size_t index = 0; index + 1 < line.size(); ++index)
            {
                if (line[index] != ':')
                {
                    continue;
                }

                const size_t digitBegin = index + 1;
                size_t digitEnd = digitBegin;
                while (digitEnd < line.size() && std::isdigit(static_cast<unsigned char>(line[digitEnd])) != 0)
                {
                    ++digitEnd;
                }

                if (digitEnd == digitBegin)
                {
                    continue;
                }

                // Either ":12: message" or a chunk name that is nothing but the location.
                if (digitEnd < line.size() && line[digitEnd] != ':')
                {
                    continue;
                }

                int lineNumber = 0;
                for (size_t digit = digitBegin; digit < digitEnd; ++digit)
                {
                    // A digit run long enough to overflow is not a line number.
                    if (lineNumber > 100000000)
                    {
                        return false;
                    }
                    lineNumber = lineNumber * 10 + (line[digit] - '0');
                }

                outLine = lineNumber;
                outFileEnd = index;
                outMessageBegin = digitEnd < line.size() ? digitEnd + 1 : digitEnd;
                return true;
            }

            return false;
        }
    }

    ScriptLocation ParseScriptError(std::string_view errorMessage, std::string_view fallbackFile)
    {
        ScriptLocation location;
        location.File = std::string(fallbackFile);

        // Only the first line is used: what follows is Lua's traceback.
        const size_t newline = errorMessage.find('\n');
        const std::string_view firstLine = Trim(
            newline == std::string_view::npos ? errorMessage : errorMessage.substr(0, newline));

        size_t fileEnd = 0;
        size_t messageBegin = 0;
        int line = 0;
        if (SplitLocationPrefix(firstLine, fileEnd, messageBegin, line))
        {
            const std::string_view rawFile = Trim(firstLine.substr(0, fileEnd));
            if (!rawFile.empty())
            {
                location.File = UnwrapChunkName(rawFile);
            }
            location.Line = line;
            location.Message = std::string(Trim(firstLine.substr(messageBegin)));
        }
        else
        {
            location.Message = std::string(firstLine);
        }

        return location;
    }

    std::string FormatScriptLocation(const ScriptLocation& location)
    {
        std::string text;
        if (!location.File.empty())
        {
            text = location.File;
            if (location.Line > 0)
            {
                text += ':';
                text += std::to_string(location.Line);
            }
            text += ": ";
        }

        text += location.Message;
        return text;
    }
}
