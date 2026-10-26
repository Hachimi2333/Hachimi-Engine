#pragma once

#include "Core/Base.h"

#include <algorithm>

namespace HachimiEngine
{
    // Frame timing service.
    //
    // The main loop used to compute a raw clock difference and hand it straight to every layer,
    // so a hitch - a breakpoint, a shader compile, a window drag - produced one enormous delta
    // that physics and scripts both consumed. Time clamps that raw delta once, here, and keeps
    // the distinction explicit:
    //
    //   UnscaledDeltaTime  clamped frame time, ignores TimeScale. Use for UI and for anything
    //                      that must keep running while the game is paused.
    //   DeltaTime          UnscaledDeltaTime * TimeScale. This is what gameplay and scripts use.
    //   FixedDeltaTime      the configured physics/script fixed step. The accumulator that turns
    //                      it into a whole number of steps per frame is StepAccumulator().
    //   ElapsedTime        unscaled total, so pausing or slowing down does not accumulate drift.
    //   ScaledElapsedTime  total time the simulation has actually experienced.
    //
    // Time is a plain object so tests can own one and drive it deterministically; Application
    // owns the instance the engine runs on and exposes it through Get().
    class Time
    {
    public:
        // Longest frame the simulation will ever see. Longer frames are treated as the maximum
        // instead, which is what stops a debugger pause from teleporting every rigid body.
        static constexpr float MaxRawDelta = 0.1f;
        // Most fixed steps one frame may consume. The debt beyond that is carried, not dropped,
        // so a slow frame makes the simulation catch up over the following frames rather than
        // running an unbounded spiral or skipping time.
        static constexpr int MaxStepsPerFrame = 4;

        Time() = default;

        // The instance the running application advances. Also the object a script or a system
        // reads through HE::Time, so no subsystem has to thread a Timestep through by hand.
        static Time& Get();

        // Advances the clock by one frame. Call exactly once per frame, from the main loop.
        void Advance(float rawDeltaSeconds);

        // Clears every accumulated value; used when a project or a runtime starts.
        void Reset();

        float GetRawDeltaTime() const { return m_RawDeltaTime; }
        float GetUnscaledDeltaTime() const { return m_UnscaledDeltaTime; }
        float GetDeltaTime() const { return m_DeltaTime; }
        float GetFixedDeltaTime() const { return m_FixedDeltaTime; }
        double GetElapsedTime() const { return m_ElapsedTime; }
        double GetScaledElapsedTime() const { return m_ScaledElapsedTime; }
        uint64_t GetFrameCount() const { return m_FrameCount; }

        float GetTimeScale() const { return m_TimeScale; }
        // Negative scales are rejected: running the simulation backwards is not supported.
        void SetTimeScale(float timeScale);

        // Fixed step the simulation uses. Clamped to a sane positive range on assignment.
        void SetFixedDeltaTime(float fixedDeltaTime);

        // Fixed steps still owed after the frame that was just advanced. Scene consumes one step
        // per FixedUpdate phase, so this is what keeps physics and FixedUpdate scripts in step
        // with each other. Capped at MaxStepsPerFrame worth of debt.
        //
        // The leftover is also the interpolation factor: rendering blends the last simulated state
        // with the next one by accumulator / FixedDeltaTime.
        float GetStepAccumulator() const { return m_StepAccumulator; }

        // Consumes one fixed step and subtracts it from the accumulator. Returns false when the
        // frame is not worth a step, so it doubles as the loop condition.
        bool ConsumeStep();

        // Drops the debt without consuming it, for when the simulation is paused or stopped.
        void ClearSteps() { m_StepAccumulator = 0.0f; }

        // Puts a specific remainder back, so a caller that consumed the frame's debt in one go can
        // restore the share no fixed step covered. Values below the fixed step are the useful ones;
        // anything larger is clamped to the catch-up cap.
        void SetStepAccumulator(float stepAccumulator)
        {
            m_StepAccumulator = std::clamp(stepAccumulator, 0.0f, m_FixedDeltaTime * static_cast<float>(MaxStepsPerFrame));
        }

    private:
        float m_RawDeltaTime = 0.0f;
        float m_UnscaledDeltaTime = 0.0f;
        float m_DeltaTime = 0.0f;
        float m_FixedDeltaTime = 1.0f / 60.0f;
        float m_TimeScale = 1.0f;
        float m_StepAccumulator = 0.0f;
        double m_ElapsedTime = 0.0;
        double m_ScaledElapsedTime = 0.0;
        uint64_t m_FrameCount = 0;
    };
}
