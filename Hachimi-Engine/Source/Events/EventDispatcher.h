#pragma once

#include "Events/Event.h"

namespace HachimiEngine
{
    // Dispatches an event to a handler only when the requested event type matches.
    class EventDispatcher
    {
    public:
        explicit EventDispatcher(Event& event)
            : m_Event(event)
        {
        }

        template<typename T, typename F>
        bool Dispatch(const F& handler)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.Handled |= handler(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        Event& m_Event;
    };
}
