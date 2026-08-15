#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Window.h"
#include "Events/ApplicationEvent.h"

namespace HachimiEngine
{
    // Engine entry point object: owns the window and drives the main loop.
    class Application
    {
    public:
        Application(const WindowProps& props = WindowProps());
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();
        void Close();

        Window& GetWindow() { return *m_Window; }
        static Application& Get() { return *s_Instance; }

    private:
        void OnEvent(Event& event);
        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);

    protected:
        Scope<Window> m_Window;
        bool m_Running = true;
        bool m_Minimized = false;

    private:
        static Application* s_Instance;
    };
}
