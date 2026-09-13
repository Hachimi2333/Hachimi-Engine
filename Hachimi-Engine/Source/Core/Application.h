#pragma once

#include "Core/Base.h"
#include "Core/LayerStack.h"
#include "Core/Memory.h"
#include "Core/Window.h"
#include "Events/ApplicationEvent.h"

namespace HachimiEngine
{
    class AssetDatabase;
    class ImGuiLayer;
    class RendererContext;
    class TextureCache;

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
        void PopLayer(Layer* layer) { m_LayerStack.PopLayer(layer); }
        void PopOverlay(Layer* overlay) { m_LayerStack.PopOverlay(overlay); }
        bool HasLayer(Layer* layer) const { return m_LayerStack.ContainsLayer(layer); }
        bool HasOverlay(Layer* overlay) const { return m_LayerStack.ContainsOverlay(overlay); }

        Window& GetWindow() { return *m_Window; }
        // Renderer-side GPU resources and frame-independent render state. Valid from the
        // constructor until the destructor, and only while a GL context is current.
        RendererContext& GetRendererContext() { return *m_RendererContext; }

        // Asset services. They outlive every project, so they live here rather than on Project:
        // opening a project only points them at a new content root. The renderer holds the same
        // two pointers, which is how a pass resolves an AssetHandle without a singleton.
        AssetDatabase& GetAssetDatabase() { return *m_AssetDatabase; }
        TextureCache& GetTextureCache() { return *m_TextureCache; }

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
        // Declared after m_Window so it is destroyed before the window and its GL
        // context, even if the destructor body is never reached.
        Scope<RendererContext> m_RendererContext;
        // Released before the renderer context so no GPU texture outlives the backend.
        Scope<TextureCache> m_TextureCache;
        Scope<AssetDatabase> m_AssetDatabase;

        static Application* s_Instance;
    };
}
