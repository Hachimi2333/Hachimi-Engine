#include "Core/Input.h"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace HachimiEngine
{
    void* Input::s_GLFWWindow = nullptr;
    InputSnapshot Input::s_Snapshot;
    bool Input::s_HasSnapshot = false;
    float Input::s_PendingScrollDelta = 0.0f;
    float Input::s_PreviousMouseX = 0.0f;
    float Input::s_PreviousMouseY = 0.0f;
    bool Input::s_HasPreviousMousePosition = false;

    void Input::SetGLFWWindow(void* window)
    {
        s_GLFWWindow = window;

        // Allocated once here rather than every frame: the snapshot is read every frame and must
        // not allocate on the hot path.
        s_Snapshot.Keys.assign(KeyCount, false);
        s_Snapshot.MouseButtons.assign(MouseButtonCount, false);
        s_Snapshot.MouseDeltaX = 0.0f;
        s_Snapshot.MouseDeltaY = 0.0f;
        s_Snapshot.ScrollDelta = 0.0f;
        s_HasSnapshot = false;
        s_PendingScrollDelta = 0.0f;
        s_HasPreviousMousePosition = false;
    }

    void Input::InvalidateGLFWWindow(void* window)
    {
        // Only clear the state if it belongs to the window being destroyed; a second window
        // shutting down later must not wipe the handle of one that is still alive.
        if (s_GLFWWindow == window)
        {
            s_GLFWWindow = nullptr;
            s_HasSnapshot = false;
            s_HasPreviousMousePosition = false;
        }
    }

    void Input::OnCursorMoved(float x, float y)
    {
        s_Snapshot.MouseX = x;
        s_Snapshot.MouseY = y;
    }

    void Input::OnScrolled(float delta)
    {
        s_PendingScrollDelta += delta;
    }

    void Input::UpdateFrame()
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);

        s_HasSnapshot = true;
        std::fill(s_Snapshot.Keys.begin(), s_Snapshot.Keys.end(), false);
        std::fill(s_Snapshot.MouseButtons.begin(), s_Snapshot.MouseButtons.end(), false);

        if (window == nullptr)
        {
            // No window: everything released. Tests and headless tools hit this path, and clearing
            // rather than keeping last frame's state is what keeps "no window" unambiguous.
            s_Snapshot.MouseDeltaX = 0.0f;
            s_Snapshot.MouseDeltaY = 0.0f;
            s_Snapshot.ScrollDelta = 0.0f;
            s_PendingScrollDelta = 0.0f;
            s_Snapshot.Gamepad = GamepadSnapshot {};
            return;
        }

        // Only codes the platform accepts are polled. GLFW reports anything below GLFW_KEY_SPACE
        // through its error callback and returns "released", so starting the loop at zero writes an
        // "Invalid key" error per code per frame straight into the editor console.
        for (size_t code = static_cast<size_t>(MinKeyCode); code < KeyCount; ++code)
        {
            s_Snapshot.Keys[code] = glfwGetKey(window, static_cast<int>(code)) == GLFW_PRESS;
        }

        for (size_t code = static_cast<size_t>(MinMouseButtonCode); code < MouseButtonCount; ++code)
        {
            s_Snapshot.MouseButtons[code] =
                glfwGetMouseButton(window, static_cast<int>(code)) == GLFW_PRESS;
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        const float cursorX = static_cast<float>(mouseX);
        const float cursorY = static_cast<float>(mouseY);
        s_Snapshot.MouseX = cursorX;
        s_Snapshot.MouseY = cursorY;

        // The first frame has no previous position to compare against; reporting the jump from
        // (0, 0) as a delta would spin the view once on startup.
        if (s_HasPreviousMousePosition)
        {
            s_Snapshot.MouseDeltaX = cursorX - s_PreviousMouseX;
            s_Snapshot.MouseDeltaY = cursorY - s_PreviousMouseY;
        }
        else
        {
            s_Snapshot.MouseDeltaX = 0.0f;
            s_Snapshot.MouseDeltaY = 0.0f;
            s_HasPreviousMousePosition = true;
        }

        s_PreviousMouseX = cursorX;
        s_PreviousMouseY = cursorY;
        s_Snapshot.ScrollDelta = s_PendingScrollDelta;
        s_PendingScrollDelta = 0.0f;

        PollGamepad();
    }

    void Input::PollGamepad()
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        s_Snapshot.Gamepad = GamepadSnapshot {};

        if (window == nullptr)
        {
            return;
        }

        // First attached gamepad wins. Supporting several at once is a co-op feature, not
        // something the input layer needs to model yet.
        for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
        {
            if (glfwJoystickPresent(joystick) != GLFW_TRUE)
            {
                continue;
            }

            GLFWgamepadstate state {};
            if (glfwGetGamepadState(joystick, &state) != GLFW_TRUE)
            {
                // Present but not recognizable as a gamepad - an unmapped joystick or a wheel.
                continue;
            }

            s_Snapshot.Gamepad.Connected = true;

            const int buttonCount = std::min(GamepadButtonCount, GLFW_GAMEPAD_BUTTON_LAST + 1);
            for (int button = 0; button < buttonCount; ++button)
            {
                s_Snapshot.Gamepad.Buttons[button] = state.buttons[button] == GLFW_PRESS;
            }

            const int axisCount = std::min(GamepadAxisCount, GLFW_GAMEPAD_AXIS_LAST + 1);
            for (int axis = 0; axis < axisCount; ++axis)
            {
                s_Snapshot.Gamepad.Axes[axis] = state.axes[axis];
            }

            return;
        }
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        if (s_HasSnapshot)
        {
            // A code the backend cannot report reads as released instead of reaching the platform.
            return IsPollableKey(keyCode) && s_Snapshot.Keys[static_cast<size_t>(keyCode)];
        }

        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        return window != nullptr && IsPollableKey(keyCode) && glfwGetKey(window, keyCode) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonPressed(MouseCode button)
    {
        if (s_HasSnapshot)
        {
            return IsPollableMouseButton(button) && s_Snapshot.MouseButtons[static_cast<size_t>(button)];
        }

        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        return window != nullptr && IsPollableMouseButton(button)
            && glfwGetMouseButton(window, button) == GLFW_PRESS;
    }

    std::pair<float, float> Input::GetMousePosition()
    {
        if (s_GLFWWindow == nullptr)
        {
            return { 0.0f, 0.0f };
        }
        return { s_Snapshot.MouseX, s_Snapshot.MouseY };
    }

    float Input::GetMouseX()
    {
        return GetMousePosition().first;
    }

    float Input::GetMouseY()
    {
        return GetMousePosition().second;
    }

    float Input::GetScrollDelta()
    {
        return s_Snapshot.ScrollDelta;
    }

    bool Input::IsCursorCaptured()
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        return window != nullptr && glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    }
}
