#include "Core/Window.h"

#include "Platform/GLFW/GLFWWindow.h"

namespace HachimiEngine
{
    Scope<Window> Window::Create(const WindowProps& props)
    {
        return CreateScope<GLFWWindow>(props);
    }
}
