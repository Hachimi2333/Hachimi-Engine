#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstddef>
#include <cstdint>

namespace HachimiEngine
{
    class PackageReader;

    // Sequential reader over one packaged entry. Blocks are inflated on demand
    // and cached, so streaming a large entry never materializes all of it.
    class PackageEntryStream
    {
    public:
        ~PackageEntryStream();

        PackageEntryStream(const PackageEntryStream&) = delete;
        PackageEntryStream& operator=(const PackageEntryStream&) = delete;

        bool IsValid() const;
        uint64_t GetSize() const;
        uint64_t GetPosition() const;

        // Reads up to size bytes. Returns the number of bytes actually read,
        // which is 0 only at end of stream.
        size_t Read(void* destination, size_t size);
        bool Seek(uint64_t position);

    private:
        friend class PackageReader;

        PackageEntryStream(const PackageReader& reader, size_t entryIndex);

        struct Impl;
        Scope<Impl> m_Impl;
    };
}
