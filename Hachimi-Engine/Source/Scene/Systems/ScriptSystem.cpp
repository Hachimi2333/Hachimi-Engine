#include "Scene/Systems/ScriptSystem.h"

#include "Scene/Scene.h"
#include "Scripting/ScriptWorld.h"

namespace HachimiEngine
{
    void ScriptSystem::OnAttach(Scene& scene)
    {
        m_ScriptWorld = CreateScope<ScriptWorld>();
        m_ScriptWorld->OnRuntimeStart(scene);
    }

    void ScriptSystem::OnUpdate(Scene& scene, Timestep timestep)
    {
        if (m_ScriptWorld == nullptr)
        {
            return;
        }

        m_ScriptWorld->OnUpdate(timestep, scene);
    }

    void ScriptSystem::OnDetach(Scene& scene)
    {
        if (m_ScriptWorld != nullptr)
        {
            m_ScriptWorld->OnRuntimeStop(scene);
            m_ScriptWorld = nullptr;
        }
    }

    bool ScriptSystem::IsRunning() const
    {
        return m_ScriptWorld != nullptr && m_ScriptWorld->IsRunning();
    }
}
