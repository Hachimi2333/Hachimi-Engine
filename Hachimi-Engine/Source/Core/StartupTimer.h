#pragma once

#include "Core/Log.h"

#include <chrono>
#include <string_view>

namespace HachimiEngine
{
    // Console-only startup measurements, independent of the simulation clock.
    class StartupTimer
    {
    public:
        using Clock = std::chrono::steady_clock;
        explicit StartupTimer(std::string_view phase) : m_Phase(phase), m_Start(Clock::now()) {}
        ~StartupTimer()
        {
            HE_CORE_INFO("[Startup] {}: {:.2f} ms", m_Phase,
                std::chrono::duration<double, std::milli>(Clock::now() - m_Start).count());
        }

        static void Begin() { s_Start = Clock::now(); s_FirstFrame = false; }
        static void FirstFrame()
        {
            if (s_FirstFrame) return;
            s_FirstFrame = true;
            HE_CORE_INFO("[Startup] First frame: {:.2f} ms",
                std::chrono::duration<double, std::milli>(Clock::now() - s_Start).count());
        }

    private:
        std::string_view m_Phase;
        Clock::time_point m_Start;
        inline static Clock::time_point s_Start = Clock::now();
        inline static bool s_FirstFrame = false;
    };
}
