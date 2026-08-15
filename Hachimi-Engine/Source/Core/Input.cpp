#include "Core/Input.h"

#include <GLFW/glfw3.h>

namespace HachimiEngine
{
    void* Input::s_GLFWWindow = nullptr;

    void Input::SetGLFWWindow(void* window)
    {
        s_GLFWWindow = window;
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        return window != nullptr && glfwGetKey(window, keyCode) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonPressed(MouseCode button)
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        return window != nullptr && glfwGetMouseButton(window, button) == GLFW_PRESS;
    }

    std::pair<float, float> Input::GetMousePosition()
    {
        auto* window = static_cast<GLFWwindow*>(s_GLFWWindow);
        if (window == nullptr)
        {
            return { 0.0f, 0.0f };
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        return { static_cast<float>(mouseX), static_cast<float>(mouseY) };
    }

    float Input::GetMouseX()
    {
        return GetMousePosition().first;
    }

    float Input::GetMouseY()
    {
        return GetMousePosition().second;
    }
}
