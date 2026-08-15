#pragma once

#include "Events/Event.h"

#include <sstream>

namespace HachimiEngine
{
    class KeyPressedEvent final : public Event
    {
    public:
        KeyPressedEvent(int keyCode, int repeatCount)
            : m_KeyCode(keyCode), m_RepeatCount(repeatCount)
        {
        }

        int GetKeyCode() const { return m_KeyCode; }
        int GetRepeatCount() const { return m_RepeatCount; }

        static EventType GetStaticType() { return EventType::KeyPressed; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "KeyPressed"; }
        int GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
            return stream.str();
        }

    private:
        int m_KeyCode;
        int m_RepeatCount;
    };

    class KeyReleasedEvent final : public Event
    {
    public:
        explicit KeyReleasedEvent(int keyCode)
            : m_KeyCode(keyCode)
        {
        }

        int GetKeyCode() const { return m_KeyCode; }

        static EventType GetStaticType() { return EventType::KeyReleased; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "KeyReleased"; }
        int GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "KeyReleasedEvent: " << m_KeyCode;
            return stream.str();
        }

    private:
        int m_KeyCode;
    };

    class KeyTypedEvent final : public Event
    {
    public:
        explicit KeyTypedEvent(int keyCode)
            : m_KeyCode(keyCode)
        {
        }

        int GetKeyCode() const { return m_KeyCode; }

        static EventType GetStaticType() { return EventType::KeyTyped; }
        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "KeyTyped"; }
        int GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
        std::string ToString() const override
        {
            std::ostringstream stream;
            stream << "KeyTypedEvent: " << m_KeyCode;
            return stream.str();
        }

    private:
        int m_KeyCode;
    };
}
