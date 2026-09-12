// XXHash: the digests stored in every package must stay stable and agree with the
// reference implementation zstd ships.

#include <doctest/doctest.h>

#include "Support/TestWorkspace.h"
#include "Utils/XXHash.h"

// The same include the engine uses: XXH_INLINE_ALL keeps the reference implementation
// private to this translation unit. The header arrives through the libzstd target, which
// Hachimi-Engine links publicly, so no third-party path is spelled out here.
#define XXH_INLINE_ALL
#include <common/xxhash.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

TEST_SUITE_BEGIN("Utils");

TEST_CASE("the wrapper agrees with the reference XXH64 implementation")
{
    std::vector<std::vector<uint8_t>> buffers;
    buffers.push_back({});
    buffers.push_back({ 0x42 });
    buffers.push_back(MakeCompressibleBytes(1024));
    buffers.push_back(MakeRandomBytes(64 * 1024, 7));

    for (size_t index = 0; index < buffers.size(); ++index)
    {
        INFO("buffer: ", index);

        const std::vector<uint8_t>& buffer = buffers[index];
        CHECK(XXHash::Hash(buffer.data(), buffer.size())
            == static_cast<uint64_t>(XXH64(buffer.data(), buffer.size(), 0)));
    }
}

TEST_CASE("HashString hashes exactly the bytes of the text")
{
    for (const char* text : { "", "a", "Data/multi_block.bin", "Assets/Meshes/Shared Meshes/Box Mesh.bin" })
    {
        INFO("text: ", text);

        const std::string value(text);
        CHECK(XXHash::HashString(value) == XXHash::Hash(value.data(), value.size()));
    }

    CHECK(XXHash::HashString("") == XXHash::Hash(nullptr, 0));
}

TEST_CASE("the streaming hasher matches the one-shot digest")
{
    const std::vector<uint8_t> bytes = MakeRandomBytes(4096 * 3 + 17, 99);
    const uint64_t expected = XXHash::Hash(bytes.data(), bytes.size());

    XXHashStream stream;
    stream.Update(nullptr, 0);

    size_t offset = 0;
    while (offset < bytes.size())
    {
        const size_t chunk = std::min<size_t>(777, bytes.size() - offset);
        stream.Update(bytes.data() + offset, chunk);
        offset += chunk;
    }
    stream.Update(nullptr, 0);

    CHECK(stream.Digest() == expected);
    CHECK(stream.Digest() == expected); // Digest() is stable

    XXHashStream empty;
    CHECK(empty.Digest() == XXHash::Hash(nullptr, 0));
}

TEST_CASE("different inputs produce different digests")
{
    std::vector<uint64_t> digests;
    for (const char* text : { "a", "b", "Data/x.bin", "Data/x.bin ", "Assets/Scripts/script_0.lua" })
    {
        digests.push_back(XXHash::HashString(text));
    }

    std::sort(digests.begin(), digests.end());
    CHECK(std::adjacent_find(digests.begin(), digests.end()) == digests.end());
    CHECK(XXHash::HashString("Data/x.bin") == XXHash::HashString("Data/x.bin"));
}

TEST_SUITE_END();
