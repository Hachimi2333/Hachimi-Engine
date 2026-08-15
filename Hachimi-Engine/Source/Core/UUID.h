#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    // Random 64-bit identifier used for entities, assets, and projects.
    class UUID
    {
    public:
        UUID();
        explicit UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID; }
        bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
        bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }
        bool operator<(const UUID& other) const { return m_UUID < other.m_UUID; }

        uint64_t GetValue() const { return m_UUID; }
        std::string ToString() const;

        static UUID Invalid() { return UUID(0); }

    private:
        uint64_t m_UUID;
    };
}

namespace std
{
    template<>
    struct hash<HE::UUID>
    {
        size_t operator()(const HE::UUID& uuid) const noexcept
        {
            return std::hash<uint64_t>()(uuid.GetValue());
        }
    };
}
