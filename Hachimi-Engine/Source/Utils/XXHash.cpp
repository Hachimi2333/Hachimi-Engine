#include "Utils/XXHash.h"

// XXH_INLINE_ALL marks the whole implementation `static inline`, so it stays
// private to this translation unit and never collides with the ZSTD_-prefixed
// copy that zstd compiles for its own use.
#define XXH_INLINE_ALL
#include <common/xxhash.h>

namespace HachimiEngine
{
    uint64_t XXHash::Hash(const void* data, size_t size)
    {
        return static_cast<uint64_t>(XXH64(data, size, 0));
    }

    uint64_t XXHash::HashString(std::string_view text)
    {
        return Hash(text.data(), text.size());
    }

    struct XXHashStream::Impl
    {
        XXH64_state_t State;
    };

    XXHashStream::XXHashStream()
        : m_Impl(CreateScope<Impl>())
    {
        XXH64_reset(&m_Impl->State, 0);
    }

    XXHashStream::~XXHashStream() = default;

    void XXHashStream::Update(const void* data, size_t size)
    {
        if (data == nullptr || size == 0)
        {
            return;
        }
        XXH64_update(&m_Impl->State, data, size);
    }

    uint64_t XXHashStream::Digest() const
    {
        return static_cast<uint64_t>(XXH64_digest(&m_Impl->State));
    }
}
