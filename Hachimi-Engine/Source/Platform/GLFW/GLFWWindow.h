#pragma once

#include "Core/Window.h"

struct GLFWwindow;

namespace HachimiEngine
{
    class GraphicsContext;

    // GLFW implementation of the engine Window interface.
    class GLFWWindow final : public Window
    {
    public:
        explicit GLFWWindow(const WindowProps& props);
        ~GLFWWindow() override;

        void OnUpdate() override;
        void SwapBuffers() override;

        uint32_t GetWidth() const override { return m_Data.Width; }
        uint32_t GetHeight() const override { return m_Data.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override { return m_Data.VSync; }

        void* GetNativeWindow() const override { return m_Window; }

    private:
        void InstallCallbacks() const;
        void Shutdown();

        struct WindowData
        {
            std::string Title;
            uint32_t Width = 0;
            uint32_t Height = 0;
            bool VSync = true;
            EventCallbackFn EventCallback;
        };

        GLFWwindow* m_Window = nullptr;
        WindowData m_Data;
        Scope<GraphicsContext> m_Context;
    };
}
