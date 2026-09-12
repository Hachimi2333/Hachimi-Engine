// UUID: the 64-bit identifier used for entities, assets and projects.

#include <doctest/doctest.h>

#include "Core/UUID.h"

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

using namespace HachimiEngine;

TEST_SUITE_BEGIN("Core");

TEST_CASE("fresh UUIDs are unique and never invalid")
{
    constexpr size_t count = 1000;

    std::set<uint64_t> values;
    size_t invalidCount = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const UUID uuid;
        if (uuid == UUID::Invalid())
        {
            ++invalidCount;
        }
        values.insert(uuid.GetValue());
    }

    CHECK(invalidCount == 0);
    CHECK(values.size() == count);
    CHECK(values.find(0) == values.end());
}

TEST_CASE("explicit UUID values round trip")
{
    const UUID zero(0);
    CHECK(zero == UUID::Invalid());
    CHECK(zero.GetValue() == 0u);
    CHECK(zero.ToString() == "0000000000000000");

    const UUID small(0x0F);
    CHECK(small == UUID(15));
    CHECK(small.GetValue() == 0x0Fu);
    CHECK(static_cast<uint64_t>(small) == 0x0Fu);
    CHECK(small.ToString() == "000000000000000f");
    CHECK(small.ToString() == small.ToString());

    const UUID allOnes(0xFFFFFFFFFFFFFFFFull);
    CHECK(allOnes.GetValue() == 0xFFFFFFFFFFFFFFFFull);
    CHECK(allOnes.ToString() == "ffffffffffffffff");

    CHECK(small != allOnes);
    CHECK(small < allOnes);
    CHECK_FALSE(allOnes < small);
}

TEST_CASE("UUIDs work as unordered_map keys")
{
    std::unordered_map<UUID, std::string> names;
    const UUID first(1);
    const UUID second(2);

    names[first] = "first";
    names[second] = "second";
    CHECK(names.size() == 2);
    CHECK(names[first] == "first");
    CHECK(names[second] == "second");

    names.erase(first);
    CHECK(names.size() == 1);
    CHECK(names.find(first) == names.end());
    CHECK(names.find(second) != names.end());
}

TEST_SUITE_END();
