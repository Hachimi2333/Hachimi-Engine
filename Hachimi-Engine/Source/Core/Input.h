#pragma once

#include "Core/Base.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"

#include <utility>

namespace HachimiEngine
{
    // Polled input state. The GLFW window backend installs its native handle on creation.
    class Input
    {
    public:
        static bool IsKeyPressed(KeyCode keyCode);
        static bool IsMouseButtonPressed(MouseCode button);
        static std::pair<float, float> GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();

    private:
        friend class GLFWWindow;
        static void SetGLFWWindow(void* window);

        static void* s_GLFWWindow;
    };
}
