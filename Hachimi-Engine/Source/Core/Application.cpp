#include "Core/Application.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Timestep.h"
#include "Events/ApplicationEvent.h"
#include "Events/EventDispatcher.h"

#include <glad/gl.h>

#include <chrono>

namespace HachimiEngine
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props)
    {
        HE_CORE_ASSERT(s_Instance == nullptr);
        s_Instance = this;

        m_Window = Window::Create(props);
        m_Window->SetEventCallback([this](Event& event) { OnEvent(event); });
    }

    Application::~Application()
    {
        s_Instance = nullptr;
    }

    void Application::Run()
    {
        using Clock = std::chrono::steady_clock;

        auto lastFrameTime = Clock::now();

        while (m_Running)
        {
            const auto now = Clock::now();
            const float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;
            const Timestep timestep(deltaTime);

            m_Window->OnUpdate();

            if (!m_Minimized)
            {
                // Temporary clear color until the renderer abstraction owns scene presentation.
                glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);

                m_Window->SwapBuffers();
            }
        }
    }

    void Application::Close()
    {
        m_Running = false;
    }

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& closeEvent) { return OnWindowClose(closeEvent); });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& resizeEvent) { return OnWindowResize(resizeEvent); });
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        Close();
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& event)
    {
        if (event.GetWidth() == 0 || event.GetHeight() == 0)
        {
            m_Minimized = true;
            return false;
        }

        m_Minimized = false;
        glViewport(0, 0, static_cast<GLsizei>(event.GetWidth()), static_cast<GLsizei>(event.GetHeight()));
        return false;
    }
}
