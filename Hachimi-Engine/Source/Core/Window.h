#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Events/Event.h"

#include <functional>
#include <string>

namespace HachimiEngine
{
    struct WindowProps
    {
        std::string Title = "Hachimi-Engine";
        uint32_t Width = 1600;
        uint32_t Height = 900;

        WindowProps() = default;
        WindowProps(std::string title, uint32_t width, uint32_t height)
            : Title(std::move(title)), Width(width), Height(height)
        {
        }
    };

    // Platform independent window interface; implemented by the GLFW backend.
    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual void OnUpdate() = 0;
        virtual void SwapBuffers() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;

        virtual void* GetNativeWindow() const = 0;

        static Scope<Window> Create(const WindowProps& props = WindowProps());
    };
}
