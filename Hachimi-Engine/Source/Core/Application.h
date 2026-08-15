#pragma once

#include "Core/Base.h"
#include "Core/LayerStack.h"
#include "Core/Memory.h"
#include "Core/Window.h"
#include "Events/ApplicationEvent.h"

namespace HachimiEngine
{
    class ImGuiLayer;

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

        void PushLayer(const Ref<Layer>& layer) { m_LayerStack.PushLayer(layer); }
        void PushOverlay(const Ref<Layer>& overlay) { m_LayerStack.PushOverlay(overlay); }
        void PopLayer(const Ref<Layer>& layer) { m_LayerStack.PopLayer(layer); }
        void PopOverlay(const Ref<Layer>& overlay) { m_LayerStack.PopOverlay(overlay); }

        Window& GetWindow() { return *m_Window; }
        static Application& Get() { return *s_Instance; }

    private:
        void OnEvent(Event& event);
        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowResize(WindowResizeEvent& event);

    protected:
        Scope<Window> m_Window;
        LayerStack m_LayerStack;
        Ref<ImGuiLayer> m_ImGuiLayer;
        bool m_Running = true;
        bool m_Minimized = false;

    private:
        static Application* s_Instance;
    };
}
