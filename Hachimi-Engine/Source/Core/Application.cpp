#include "Core/Application.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Timestep.h"
#include "Events/ApplicationEvent.h"
#include "Events/EventDispatcher.h"
#include "Renderer/RenderCommand.h"

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

        RenderCommand::Init();
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.10f, 1.0f });
    }

    Application::~Application()
    {
        RenderCommand::SetDepthTest(false);
        Renderer::Shutdown();
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

            AppUpdateEvent updateEvent;
            OnEvent(updateEvent);
            m_LayerStack.Update(timestep);

            if (!m_Minimized)
            {
                AppRenderEvent renderEvent;
                OnEvent(renderEvent);

                RenderCommand::Clear();
                m_LayerStack.RenderImGui();
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

        m_LayerStack.DispatchEvent(event);
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
        RenderCommand::SetViewport(0, 0, event.GetWidth(), event.GetHeight());
        return false;
    }
}
