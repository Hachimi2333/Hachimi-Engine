#include "Platform/GLFW/GLFWWindow.h"

#include "Core/Assert.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Events/ApplicationEvent.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>

namespace HachimiEngine
{
    namespace
    {
        void GLFWErrorCallback(int error, const char* description)
        {
            HE_CORE_ERROR("GLFW Error ({}): {}", error, description);
        }
    }

    GLFWWindow::GLFWWindow(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        HE_CORE_INFO("Creating window {} ({}x{})", props.Title, props.Width, props.Height);

        const int initSuccess = glfwInit();
        HE_CORE_ASSERT(initSuccess == GLFW_TRUE);

        glfwSetErrorCallback(GLFWErrorCallback);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        m_Window = glfwCreateWindow(static_cast<int>(props.Width), static_cast<int>(props.Height), props.Title.c_str(), nullptr, nullptr);
        HE_CORE_ASSERT(m_Window != nullptr);

        m_Context = CreateScope<OpenGLContext>(m_Window);
        m_Context->Init();

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);
        InstallCallbacks();
        Input::SetGLFWWindow(m_Window);
    }

    GLFWWindow::~GLFWWindow()
    {
        Shutdown();
    }

    void GLFWWindow::OnUpdate()
    {
        glfwPollEvents();
        // Sample after polling so gameplay sees this frame's buttons, cursor and wheel together.
        Input::UpdateFrame();
    }

    void GLFWWindow::SwapBuffers()
    {
        m_Context->SwapBuffers();
    }

    void GLFWWindow::SetVSync(bool enabled)
    {
        glfwSwapInterval(enabled ? 1 : 0);
        m_Data.VSync = enabled;
    }

    void GLFWWindow::SetCursorMode(CursorMode mode)
    {
        if (m_Data.CurrentCursorMode == mode) return;
        Input::s_HasPreviousMousePosition = false;
        m_Data.CurrentCursorMode = mode;

        if (m_Window == nullptr)
        {
            return;
        }

        int glfwMode = GLFW_CURSOR_NORMAL;
        switch (mode)
        {
            case CursorMode::Normal:
                glfwMode = GLFW_CURSOR_NORMAL;
                break;
            case CursorMode::Hidden:
                glfwMode = GLFW_CURSOR_HIDDEN;
                break;
            case CursorMode::Disabled:
                glfwMode = GLFW_CURSOR_DISABLED;
                break;
        }

        glfwSetInputMode(m_Window, GLFW_CURSOR, glfwMode);
    }

    void GLFWWindow::SetFullscreen(bool fullscreen)
    {
        if (m_Window == nullptr || m_Data.Fullscreen == fullscreen)
        {
            return;
        }

        if (fullscreen)
        {
            glfwGetWindowPos(m_Window, &m_Data.WindowedX, &m_Data.WindowedY);
            m_Data.WindowedWidth = m_Data.Width;
            m_Data.WindowedHeight = m_Data.Height;

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (monitor == nullptr)
            {
                HE_CORE_WARN("Cannot switch to full screen: no primary monitor was reported");
                return;
            }

            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode == nullptr)
            {
                HE_CORE_WARN("Cannot switch to full screen: the monitor reported no video mode");
                return;
            }

            glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            const uint32_t width = m_Data.WindowedWidth > 0 ? m_Data.WindowedWidth : m_Data.Width;
            const uint32_t height = m_Data.WindowedHeight > 0 ? m_Data.WindowedHeight : m_Data.Height;
            glfwSetWindowMonitor(m_Window, nullptr, m_Data.WindowedX, m_Data.WindowedY, static_cast<int>(width), static_cast<int>(height), 0);
        }

        m_Data.Fullscreen = fullscreen;
    }

    void GLFWWindow::SetTitle(const std::string& title)
    {
        m_Data.Title = title;
        if (m_Window != nullptr)
        {
            glfwSetWindowTitle(m_Window, title.c_str());
        }
    }

    void GLFWWindow::InstallCallbacks() const
    {
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.Width = static_cast<uint32_t>(width);
            data.Height = static_cast<uint32_t>(height);

            WindowResizeEvent event(data.Width, data.Height);
            data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
                default:
                    break;
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            KeyTypedEvent event(static_cast<int>(keyCode));
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                default:
                    break;
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            Input::OnScrolled(static_cast<float>(yOffset));

            MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPosition, double yPosition)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            Input::OnCursorMoved(static_cast<float>(xPosition), static_cast<float>(yPosition));

            MouseMovedEvent event(static_cast<float>(xPosition), static_cast<float>(yPosition));
            data.EventCallback(event);
        });
    }

    void GLFWWindow::Shutdown()
    {
        // Input loses its handle first: the callbacks below are uninstalled with the window, and a
        // poll that ran in between would otherwise read a destroyed one.
        Input::InvalidateGLFWWindow(m_Window);

        if (m_Window != nullptr)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }

        glfwTerminate();
    }
}
