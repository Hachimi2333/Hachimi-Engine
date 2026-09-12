#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace HachimiEngine
{
    // Read-only view over file bytes. Holds either a borrowed view into a package
    // memory mapping (zero copy for stored entries) or an owned decompressed
    // buffer, so callers handle both cases through one type.
    class FileMapping
    {
    public:
        FileMapping() = default;

        // View into memory owned elsewhere; the caller must outlive this object.
        //
        // owner is what keeps that memory alive: a view borrowed from a memory mapped package
        // used to dangle as soon as the mount was released, even though the mapping was still
        // the only reference to it. Passing the owner makes the view self-sufficient.
        static FileMapping Borrow(const void* data, size_t size, std::shared_ptr<const void> owner = nullptr)
        {
            FileMapping mapping;
            mapping.m_Borrowed = static_cast<const uint8_t*>(data);
            mapping.m_Size = size;
            mapping.m_Owner = std::move(owner);
            mapping.m_Valid = true;
            return mapping;
        }

        // Owns decompressed bytes.
        static FileMapping Own(std::vector<uint8_t> data)
        {
            FileMapping mapping;
            mapping.m_Size = data.size();
            mapping.m_Owned = std::move(data);
            mapping.m_Valid = true;
            return mapping;
        }

        bool IsValid() const { return m_Valid; }
        const uint8_t* Data() const { return m_Borrowed != nullptr ? m_Borrowed : m_Owned.data(); }
        size_t Size() const { return m_Size; }

        void Reset()
        {
            m_Borrowed = nullptr;
            m_Owner.reset();
            m_Owned.clear();
            m_Owned.shrink_to_fit();
            m_Size = 0;
            m_Valid = false;
        }

    private:
        const uint8_t* m_Borrowed = nullptr;
        // Empty for an owned mapping.
        std::shared_ptr<const void> m_Owner;
        std::vector<uint8_t> m_Owned;
        size_t m_Size = 0;
        bool m_Valid = false;
    };
}
