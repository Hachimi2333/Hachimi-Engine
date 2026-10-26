#pragma once

#include "Core/Base.h"

#include <algorithm>

namespace HachimiEngine
{
    // Time delta used by the update loop, expressed in seconds.
    //
    // Deliberately still a small value type: it is what a layer, a system or a test passes around,
    // while Core/Time.h owns the frame clock and the fixed-step accumulator.
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

        // Same delta, never above maxSeconds. A frame that took longer - a breakpoint, a stall, a
        // window drag - is reported as the cap so one frame cannot teleport the simulation.
        Timestep Clamp(float maxSeconds) const
        {
            return Timestep(std::min(m_Time, maxSeconds));
        }

    private:
        float m_Time;
    };
}
