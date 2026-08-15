#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    // Time delta used by the update loop, expressed in seconds.
    class Timestep
    {
    public:
        Timestep(float time = 0.0f)
            : m_Time(time)
        {
        }

        operator float() const { return m_Time; }

        float GetSeconds() const { return m_Time; }
        float GetMilliseconds() const { return m_Time * 1000.0f; }

    private:
        float m_Time;
    };
}
