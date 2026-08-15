#include "Core/UUID.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace HachimiEngine
{
    namespace
    {
        std::mt19937_64& GetRandomEngine()
        {
            static std::mt19937_64 engine(std::random_device{}());
            return engine;
        }
    }

    UUID::UUID()
        : m_UUID(GetRandomEngine()())
    {
    }

    UUID::UUID(uint64_t uuid)
        : m_UUID(uuid)
    {
    }

    std::string UUID::ToString() const
    {
        std::ostringstream stream;
        stream << std::hex << std::setw(16) << std::setfill('0') << m_UUID;
        return stream.str();
    }
}
