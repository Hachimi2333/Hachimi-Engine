#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace HachimiEngine
{
    // One enumerator together with the name it is stored under.
    template<typename E>
    struct EnumEntry
    {
        E Value;
        std::string_view Name;
    };

    // Scene files store enumerators by name rather than by numeric value.
    //
    // The numeric form made the format depend on declaration order: inserting an enumerator
    // silently reinterpreted every existing scene, and the Inspector even derived a combo index
    // by subtracting one enumerator from another. Names cost a few bytes and remove that class
    // of data corruption entirely.
    namespace EnumNames
    {
        inline constexpr std::string_view UnknownName = "Unknown";

        template<typename E, size_t N>
        std::string_view ToName(E value, const std::array<EnumEntry<E>, N>& entries)
        {
            for (const EnumEntry<E>& entry : entries)
            {
                if (entry.Value == value)
                {
                    return entry.Name;
                }
            }
            return UnknownName;
        }

        // Leaves outValue untouched and returns false for an unrecognised name, so the caller
        // can keep its default instead of silently taking a wrong value.
        template<typename E, size_t N>
        bool FromName(std::string_view name, const std::array<EnumEntry<E>, N>& entries, E& outValue)
        {
            for (const EnumEntry<E>& entry : entries)
            {
                if (entry.Name == name)
                {
                    outValue = entry.Value;
                    return true;
                }
            }
            return false;
        }
    }
}
