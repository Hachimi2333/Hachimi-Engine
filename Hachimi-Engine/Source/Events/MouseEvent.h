#pragma once

#include "Events/Event.h"

#include <sstream>

namespace HachimiEngine
{
    class MouseMovedEvent final : public Event
    {
    public:
        MouseMovedEvent(float x, float y)
            : m_MouseX(x), m_MouseY(y)
        {
        }

        float GetX() const { return m_MouseX; }
        float GetY() const { return m_MouseY; }

        static EventType GetStaticType() { return EventType::MouseMoved; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseMoved"; }
        int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
            return stream.str();
        }

    private:
        float m_MouseX;
        float m_MouseY;
    };

    class MouseScrolledEvent final : public Event
    {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : m_XOffset(xOffset), m_YOffset(yOffset)
        {
        }

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }

        static EventType GetStaticType() { return EventType::MouseScrolled; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseScrolled"; }
        int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "MouseScrolledEvent: " << m_XOffset << ", " << m_YOffset;
            return stream.str();
        }

    private:
        float m_XOffset;
        float m_YOffset;
    };

    class MouseButtonPressedEvent final : public Event
    {
    public:
        explicit MouseButtonPressedEvent(int button)
            : m_Button(button)
        {
        }

        int GetMouseButton() const { return m_Button; }

        static EventType GetStaticType() { return EventType::MouseButtonPressed; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseButtonPressed"; }
        int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "MouseButtonPressedEvent: " << m_Button;
            return stream.str();
        }

    private:
        int m_Button;
    };

    class MouseButtonReleasedEvent final : public Event
    {
    public:
        explicit MouseButtonReleasedEvent(int button)
            : m_Button(button)
        {
        }

        int GetMouseButton() const { return m_Button; }

        static EventType GetStaticType() { return EventType::MouseButtonReleased; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseButtonReleased"; }
        int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "MouseButtonReleasedEvent: " << m_Button;
            return stream.str();
        }

    private:
        int m_Button;
    };
}
