#pragma once

#include "Core/Base.h"
#include "Core/InputMap.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"

#include <cstdint>
#include <utility>

namespace HachimiEngine
{
    // Polled input state. The GLFW window backend installs its native handle on creation and
    // forwards cursor and scroll events as they arrive.
    //
    // Raw device polling is a static class, which is what scripts and editor panels already use.
    // InputMap is the layer above it: it turns "is W down" into "is the player moving forward", and
    // it is evaluated from one snapshot per frame rather than asking the device on every query.
    class Input
    {
    public:
        // Size of the two button arrays in a snapshot, i.e. one past the highest code the backend
        // can report, so any KeyCode or MouseCode is a valid index and a query never needs a bounds
        // check at the call site.
        static constexpr size_t KeyCount = 349;
        static constexpr size_t MouseButtonCount = 8;

        // Lowest code a platform may be asked about. These mirror GLFW_KEY_SPACE and GLFW_MOUSE_BUTTON_1,
        // which are not zero: a code below them is an error at the platform layer rather than a
        // "not pressed" answer, so neither the frame poll nor a direct query may start below here.
        static constexpr int MinKeyCode = 32;
        static constexpr int MinMouseButtonCode = 0;

        // True when the code is one the backend will accept, i.e. it is inside both the snapshot
        // array and the platform's valid range.
        static bool IsPollableKey(KeyCode keyCode)
        {
            return keyCode >= MinKeyCode && static_cast<size_t>(keyCode) < KeyCount;
        }

        static bool IsPollableMouseButton(MouseCode button)
        {
            return button >= MinMouseButtonCode && static_cast<size_t>(button) < MouseButtonCount;
        }

        static bool IsKeyPressed(KeyCode keyCode);
        static bool IsMouseButtonPressed(MouseCode button);
        static std::pair<float, float> GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();
        static float GetScrollDelta();

        // True while the cursor is captured by the window (relative mouse mode).
        static bool IsCursorCaptured();

        // Gamepad state from the last frame. When no gamepad is attached the state reports
        // Connected = false and every button and axis reads as zero.
        static const GamepadSnapshot& GetGamepad() { return s_Snapshot.Gamepad; }
        static const GamepadSnapshot& GetGamepadState() { return GetGamepad(); }

        // One frame of device state, reused between frames so a per-frame poll allocates nothing.
        static const InputSnapshot& GetSnapshot() { return s_Snapshot; }

        // Samples every device once. Called by the window backend at the start of a frame; safe to
        // call with no window at all, in which case everything reads as released.
        static void UpdateFrame();

    private:
        friend class GLFWWindow;

        static void SetGLFWWindow(void* window);
        static void InvalidateGLFWWindow(void* window);
        // Records a cursor move, so the snapshot can report a per-frame delta.
        static void OnCursorMoved(float x, float y);
        // Records wheel notches, which are events rather than polled state.
        static void OnScrolled(float delta);

        static void PollGamepad();

        static void* s_GLFWWindow;
        static InputSnapshot s_Snapshot;
        // False until the first UpdateFrame, so a query made before the loop starts still asks the
        // device instead of reading an all-released snapshot.
        static bool s_HasSnapshot;
        static float s_PendingScrollDelta;
        static float s_PreviousMouseX;
        static float s_PreviousMouseY;
        static bool s_HasPreviousMousePosition;
    };
}
