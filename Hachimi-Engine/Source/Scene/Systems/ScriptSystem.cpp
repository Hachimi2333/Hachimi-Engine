#include "Scene/Systems/ScriptSystem.h"

#include "Scene/Scene.h"
#include "Scripting/ScriptWorld.h"

namespace HachimiEngine
{
    struct ScriptSystem::Impl
    {
        Scope<ScriptWorld> World;
    };

    ScriptSystem::ScriptSystem()
        : m_Impl(CreateScope<Impl>())
    {
    }

    ScriptSystem::~ScriptSystem() = default;

    void ScriptSystem::OnAttach(Scene& scene)
    {
        m_Impl->World = CreateScope<ScriptWorld>();
        m_Impl->World->OnRuntimeStart(scene);
    }

    void ScriptSystem::OnUpdate(Scene& scene, Timestep timestep)
    {
        if (m_Impl->World == nullptr)
        {
            return;
        }

        m_Impl->World->OnUpdate(timestep, scene);
    }

    void ScriptSystem::OnDetach(Scene& scene)
    {
        if (m_Impl->World != nullptr)
        {
            m_Impl->World->OnRuntimeStop(scene);
            m_Impl->World = nullptr;
        }
    }

    bool ScriptSystem::IsRunning() const
    {
        return m_Impl->World != nullptr && m_Impl->World->IsRunning();
    }
}
