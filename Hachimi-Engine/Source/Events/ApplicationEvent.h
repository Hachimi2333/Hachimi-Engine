#pragma once

#include "Events/Event.h"

#include <sstream>

namespace HachimiEngine
{
    class WindowResizeEvent final : public Event
    {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height)
            : m_Width(width), m_Height(height)
        {
        }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        static EventType GetStaticType() { return EventType::WindowResize; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowResize"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "WindowResizeEvent: " << m_Width << ", " << m_Height;
            return stream.str();
        }

    private:
        uint32_t m_Width;
        uint32_t m_Height;
    };

    class WindowCloseEvent final : public Event
    {
    public:
        WindowCloseEvent() = default;

        static EventType GetStaticType() { return EventType::WindowClose; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowClose"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }
    };

    class AppTickEvent final : public Event
    {
    public:
        AppTickEvent() = default;

        static EventType GetStaticType() { return EventType::AppTick; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "AppTick"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }
    };

    class AppUpdateEvent final : public Event
    {
    public:
        AppUpdateEvent() = default;

        static EventType GetStaticType() { return EventType::AppUpdate; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "AppUpdate"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }
    };

    class AppRenderEvent final : public Event
    {
    public:
        AppRenderEvent() = default;

        static EventType GetStaticType() { return EventType::AppRender; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "AppRender"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }
    };
}
