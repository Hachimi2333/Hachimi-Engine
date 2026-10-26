#include "Core/Time.h"

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        // Fixed step has to stay a usable positive number: a zero step would spin the fixed
        // update loop forever.
        constexpr float MinFixedDeltaTime = 1.0f / 1000.0f;
        constexpr float MaxFixedDeltaTime = 1.0f;

        // constinit, not a bare static: Time's defaults are member initializers, and without
        // constant initialization a translation unit that read the clock before main() ran would
        // see a zeroed 60 Hz step and a zero time scale.
        constinit Time s_Instance {};
    }

    Time& Time::Get()
    {
        return s_Instance;
    }

    void Time::Advance(float rawDeltaSeconds)
    {
        // A negative delta can only come from a clock that went backwards; treat it as no time.
        m_RawDeltaTime = std::isfinite(rawDeltaSeconds) ? std::max(rawDeltaSeconds, 0.0f) : 0.0f;
        m_UnscaledDeltaTime = std::min(m_RawDeltaTime, MaxRawDelta);
        m_DeltaTime = m_UnscaledDeltaTime * m_TimeScale;

        // Unscaled totals, so a paused or slowed-down simulation does not accumulate drift.
        m_ElapsedTime += m_RawDeltaTime;
        m_ScaledElapsedTime += m_DeltaTime;

        ++m_FrameCount;

        // The accumulator follows the scaled delta: TimeScale 0 freezes the simulation while the
        // clock and the frame counter keep running.
        m_StepAccumulator += m_DeltaTime;
        m_StepAccumulator = std::min(m_StepAccumulator, m_FixedDeltaTime * static_cast<float>(MaxStepsPerFrame));
    }

    void Time::Reset()
    {
        m_RawDeltaTime = 0.0f;
        m_UnscaledDeltaTime = 0.0f;
        m_DeltaTime = 0.0f;
        m_StepAccumulator = 0.0f;
        m_ElapsedTime = 0.0;
        m_ScaledElapsedTime = 0.0;
        m_FrameCount = 0;
    }

    void Time::SetTimeScale(float timeScale)
    {
        m_TimeScale = std::isfinite(timeScale) ? std::max(timeScale, 0.0f) : 0.0f;
    }

    void Time::SetFixedDeltaTime(float fixedDeltaTime)
    {
        if (std::isfinite(fixedDeltaTime))
            m_FixedDeltaTime = std::clamp(fixedDeltaTime, MinFixedDeltaTime, MaxFixedDeltaTime);
    }

    bool Time::ConsumeStep()
    {
        if (m_StepAccumulator < m_FixedDeltaTime)
        {
            return false;
        }

        m_StepAccumulator -= m_FixedDeltaTime;
        return true;
    }
}
