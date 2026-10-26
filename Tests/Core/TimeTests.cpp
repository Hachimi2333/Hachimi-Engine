// Time: the frame clock, the time scale and the fixed-step accumulator.
//
// The main loop used to hand a raw clock difference straight to every layer, so a hitch was
// delivered to physics and scripts as one enormous delta. These cases pin the clamping, the
// scaled/unscaled split and the accumulator arithmetic, all without a window or a running
// Application: Time is a plain object a test can drive frame by frame.

#include <doctest/doctest.h>
#include <limits>

#include "Core/Time.h"

#include <cmath>

using namespace HachimiEngine;

namespace
{
    bool Near(float lhs, float rhs, float tolerance = 1e-5f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool NearDouble(double lhs, double rhs, double tolerance = 1e-6)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }
}

TEST_SUITE("Time")
{
    TEST_CASE("a normal frame passes its delta through unchanged")
    {
        Time time;
        time.Advance(1.0f / 60.0f);

        CHECK(Near(time.GetUnscaledDeltaTime(), 1.0f / 60.0f));
        CHECK(Near(time.GetDeltaTime(), 1.0f / 60.0f));
        CHECK(time.GetFrameCount() == 1);
    }

    TEST_CASE("a long frame is clamped, and the raw value is still reported")
    {
        Time time;
        time.Advance(5.0f);

        CHECK(Near(time.GetRawDeltaTime(), 5.0f));
        CHECK(Near(time.GetUnscaledDeltaTime(), Time::MaxRawDelta));
        CHECK(Near(time.GetDeltaTime(), Time::MaxRawDelta));
    }

    TEST_CASE("a negative delta counts as no time at all")
    {
        Time time;
        time.Advance(-1.0f);

        CHECK(Near(time.GetRawDeltaTime(), 0.0f));
        CHECK(Near(time.GetUnscaledDeltaTime(), 0.0f));
        CHECK(time.GetFrameCount() == 1);
    }

    TEST_CASE("time scale changes the scaled delta but not the unscaled one")
    {
        Time time;
        time.SetTimeScale(0.5f);
        time.Advance(1.0f / 30.0f);

        CHECK(Near(time.GetUnscaledDeltaTime(), 1.0f / 30.0f));
        CHECK(Near(time.GetDeltaTime(), 1.0f / 60.0f));
    }

    TEST_CASE("a negative time scale is rejected rather than running backwards")
    {
        Time time;
        time.SetTimeScale(-2.0f);

        CHECK(Near(time.GetTimeScale(), 0.0f));

        time.Advance(1.0f / 60.0f);
        CHECK(Near(time.GetDeltaTime(), 0.0f));
        CHECK(Near(time.GetUnscaledDeltaTime(), 1.0f / 60.0f));
    }

    TEST_CASE("a zero time scale freezes the simulation while the clock keeps running")
    {
        Time time;
        time.SetTimeScale(0.0f);

        for (int frame = 0; frame < 10; ++frame)
        {
            time.Advance(1.0f / 60.0f);
        }

        CHECK(time.GetFrameCount() == 10);
        CHECK(time.GetStepAccumulator() < time.GetFixedDeltaTime());
        CHECK_FALSE(time.ConsumeStep());
    }

    TEST_CASE("elapsed time is unscaled, so pausing does not accumulate drift")
    {
        Time time;
        time.SetTimeScale(0.0f);
        time.Advance(0.25f);
        time.Advance(0.25f);

        // Raw total, even though the simulation experienced no time at all.
        CHECK(NearDouble(time.GetElapsedTime(), 0.5));
        CHECK(NearDouble(time.GetScaledElapsedTime(), 0.0));
    }

    TEST_CASE("the fixed step is clamped to a usable range")
    {
        Time time;

        time.SetFixedDeltaTime(0.0f);
        CHECK(time.GetFixedDeltaTime() > 0.0f);

        time.SetFixedDeltaTime(-1.0f);
        CHECK(time.GetFixedDeltaTime() > 0.0f);

        time.SetFixedDeltaTime(1000.0f);
        CHECK(time.GetFixedDeltaTime() <= 1.0f);
    }

    TEST_CASE("a fixed step is owed exactly when a whole step of time has passed")
    {
        Time time;
        time.SetFixedDeltaTime(0.01f);

        // Half a step: nothing owed yet.
        time.Advance(0.005f);
        CHECK_FALSE(time.ConsumeStep());

        // The other half completes it, and the first call consumes it.
        time.Advance(0.005f);
        CHECK(time.ConsumeStep());
        CHECK_FALSE(time.ConsumeStep());
    }

    TEST_CASE("one long frame owes several steps but never more than the catch-up cap")
    {
        Time time;
        // A step that no float sum of the frame delta lands exactly on, so the assertion is about
        // the cap rather than about floating-point luck.
        time.SetFixedDeltaTime(1.0f / 120.0f);

        // Many steps' worth in one frame, against a cap of four.
        time.Advance(0.1f);

        int steps = 0;
        while (time.ConsumeStep())
        {
            ++steps;
        }

        CHECK(steps == Time::MaxStepsPerFrame);
        CHECK(time.GetStepAccumulator() < time.GetFixedDeltaTime());
    }

    TEST_CASE("a paused accumulator can be dropped without consuming it")
    {
        Time time;
        time.SetFixedDeltaTime(1.0f / 60.0f);
        time.Advance(1.0f / 30.0f);

        CHECK(time.GetStepAccumulator() > 0.0f);

        time.ClearSteps();
        CHECK(Near(time.GetStepAccumulator(), 0.0f));
        CHECK_FALSE(time.ConsumeStep());
    }

    TEST_CASE("reset clears the clock but leaves the configuration alone")
    {
        Time time;
        time.SetTimeScale(2.0f);
        time.SetFixedDeltaTime(0.02f);
        time.Advance(0.05f);

        time.Reset();

        CHECK(time.GetFrameCount() == 0);
        CHECK(Near(time.GetRawDeltaTime(), 0.0f));
        CHECK(Near(time.GetUnscaledDeltaTime(), 0.0f));
        CHECK(Near(time.GetDeltaTime(), 0.0f));
        CHECK(Near(time.GetStepAccumulator(), 0.0f));
        CHECK(NearDouble(time.GetElapsedTime(), 0.0));
        CHECK(Near(time.GetTimeScale(), 2.0f));
        CHECK(Near(time.GetFixedDeltaTime(), 0.02f));
    }

    TEST_CASE("the process-wide clock starts at a usable default")
    {
        // Constant-initialized on purpose: a translation unit that reads the clock before main()
        // must not see a zeroed fixed step.
        CHECK(Time::Get().GetFixedDeltaTime() > 0.0f);
        CHECK(Near(Time::Get().GetTimeScale(), 1.0f));
    }
}

TEST_SUITE("Core")
{
    TEST_CASE("nonfinite clock inputs cannot poison the fixed step loop")
    {
        Time clock;
        clock.SetTimeScale(std::numeric_limits<float>::quiet_NaN());
        clock.Advance(0.1f);
        CHECK(clock.GetDeltaTime() == 0.0f);
        CHECK_FALSE(clock.ConsumeStep());
        clock.SetTimeScale(1.0f);
        clock.SetFixedDeltaTime(std::numeric_limits<float>::infinity());
        CHECK(clock.GetFixedDeltaTime() == doctest::Approx(1.0f / 60.0f));
        clock.Advance(std::numeric_limits<float>::quiet_NaN());
        CHECK(clock.GetUnscaledDeltaTime() == 0.0f);
        CHECK_FALSE(clock.ConsumeStep());
    }
}
