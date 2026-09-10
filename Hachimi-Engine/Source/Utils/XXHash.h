#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace HachimiEngine
{
    // 64-bit non-cryptographic hashing used for package path lookup, entry
    // content verification and package identity. Backed by xxHash, which zstd
    // already vendors, so no extra third-party library is required.
    class XXHash
    {
    public:
        static uint64_t Hash(const void* data, size_t size);
        static uint64_t HashString(std::string_view text);
    };

    // Incremental hasher for entries that are hashed while streaming, so the
    // whole payload never has to be resident at once.
    class XXHashStream
    {
    public:
        XXHashStream();
        ~XXHashStream();

        XXHashStream(const XXHashStream&) = delete;
        XXHashStream& operator=(const XXHashStream&) = delete;

        void Update(const void* data, size_t size);
        // Finalizes the digest. Update() must not be called afterwards.
        uint64_t Digest() const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
