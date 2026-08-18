#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptRuntime.h"

#include <vector>

namespace HachimiEngine
{
    class Scene;
    class ScriptEngine;

    // Scene-local script session created for the duration of Play mode.
    // It owns one ScriptRuntime per language and feeds ScriptComponent slots
    // into the runtime that matches the script file extension.
    class ScriptWorld
    {
    public:
        void OnRuntimeStart(Scene& scene);
        void OnUpdate(Timestep timestep, Scene& scene);
        void OnRuntimeStop(Scene& scene);

        bool IsRunning() const { return m_IsRunning; }

    private:
        struct RuntimeEntry
        {
            ScriptEngine* Engine = nullptr;
            Scope<ScriptRuntime> Runtime;
        };

    private:
        std::vector<RuntimeEntry> m_Runtimes;
        bool m_IsRunning = false;
    };
}
