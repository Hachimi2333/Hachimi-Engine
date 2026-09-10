#include "Packaging/PackageEntryStream.h"

#include "Packaging/PackageReader.h"

#include <algorithm>
#include <cstring>

namespace HachimiEngine
{
    struct PackageEntryStream::Impl
    {
        const PackageReader* Reader = nullptr;
        size_t EntryIndex = 0;
        uint64_t Size = 0;
        uint64_t Position = 0;
        bool Valid = false;

        // One decompressed block at a time, keyed by its index.
        uint32_t CachedBlock = 0;
        bool HasCachedBlock = false;
        std::vector<uint8_t> CachedBytes;
    };

    PackageEntryStream::PackageEntryStream(const PackageReader& reader, size_t entryIndex)
        : m_Impl(CreateScope<Impl>())
    {
        m_Impl->Reader = &reader;
        m_Impl->EntryIndex = entryIndex;

        if (entryIndex >= reader.GetEntryCount())
        {
            return;
        }

        m_Impl->Size = reader.GetEntry(entryIndex).UncompressedSize;
        m_Impl->Valid = true;
    }

    PackageEntryStream::~PackageEntryStream() = default;

    bool PackageEntryStream::IsValid() const
    {
        return m_Impl->Valid;
    }

    uint64_t PackageEntryStream::GetSize() const
    {
        return m_Impl->Size;
    }

    uint64_t PackageEntryStream::GetPosition() const
    {
        return m_Impl->Position;
    }

    size_t PackageEntryStream::Read(void* destination, size_t size)
    {
        if (!m_Impl->Valid || destination == nullptr || size == 0 || m_Impl->Position >= m_Impl->Size)
        {
            return 0;
        }

        const uint32_t blockCount = m_Impl->Reader->GetEntry(m_Impl->EntryIndex).BlockCount;
        if (blockCount == 0)
        {
            return 0;
        }

        uint8_t* output = static_cast<uint8_t*>(destination);
        size_t totalRead = 0;

        while (totalRead < size && m_Impl->Position < m_Impl->Size)
        {
            // Blocks are stored in uncompressed order. Streaming is sequential,
            // so resuming the scan at the cached block keeps this amortized O(1).
            uint32_t blockIndex = m_Impl->HasCachedBlock ? m_Impl->CachedBlock : 0;
            uint64_t blockOffset = 0;
            uint64_t blockSize = 0;
            if (!m_Impl->Reader->GetBlockRange(m_Impl->EntryIndex, blockIndex, blockOffset, blockSize))
            {
                break;
            }

            bool located = false;
            for (;;)
            {
                if (m_Impl->Position < blockOffset + blockSize)
                {
                    located = true;
                    break;
                }

                if (blockIndex + 1 >= blockCount)
                {
                    break;
                }

                uint64_t nextOffset = 0;
                uint64_t nextSize = 0;
                if (!m_Impl->Reader->GetBlockRange(m_Impl->EntryIndex, blockIndex + 1, nextOffset, nextSize))
                {
                    break;
                }

                ++blockIndex;
                blockOffset = nextOffset;
                blockSize = nextSize;
            }

            if (!located || blockSize == 0)
            {
                break;
            }

            if (!m_Impl->HasCachedBlock || m_Impl->CachedBlock != blockIndex)
            {
                if (!m_Impl->Reader->ReadEntryRange(m_Impl->EntryIndex, blockOffset, blockSize, m_Impl->CachedBytes))
                {
                    break;
                }
                m_Impl->CachedBlock = blockIndex;
                m_Impl->HasCachedBlock = true;
            }

            const uint64_t offsetInBlock = m_Impl->Position - blockOffset;
            if (offsetInBlock >= m_Impl->CachedBytes.size())
            {
                break;
            }

            const size_t available = m_Impl->CachedBytes.size() - static_cast<size_t>(offsetInBlock);
            const size_t chunk = std::min({ available, size - totalRead, static_cast<size_t>(m_Impl->Size - m_Impl->Position) });
            if (chunk == 0)
            {
                break;
            }

            std::memcpy(output + totalRead, m_Impl->CachedBytes.data() + offsetInBlock, chunk);
            totalRead += chunk;
            m_Impl->Position += chunk;
        }

        return totalRead;
    }

    bool PackageEntryStream::Seek(uint64_t position)
    {
        if (!m_Impl->Valid || position > m_Impl->Size)
        {
            return false;
        }

        // The cached block only helps a forward scan, so seeking invalidates it.
        // Without this a seek back to the start could never locate block 0.
        m_Impl->HasCachedBlock = false;
        m_Impl->Position = position;
        return true;
    }
}
