#pragma once

#include "Core/Base.h"
#include "Core/Timestep.h"
#include "Events/Event.h"

#include <string>

namespace HachimiEngine
{
    // Base class for every updateable/renderable layer owned by the Application.
    class Layer
    {
    public:
        explicit Layer(std::string name = "Layer")
            : m_DebugName(std::move(name))
        {
        }

        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(Timestep timestep) {}
        virtual void OnImGuiRender() {}
        virtual void OnEvent(Event& event) {}

        const std::string& GetName() const { return m_DebugName; }

    protected:
        std::string m_DebugName;
    };
}
