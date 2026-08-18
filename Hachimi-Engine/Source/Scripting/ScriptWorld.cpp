#include "Scripting/ScriptWorld.h"

#include "Core/Log.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptRuntime.h"

namespace HachimiEngine
{
    namespace
    {
        std::string GetEntityDisplayName(Entity entity)
        {
            if (entity.HasComponent<TagComponent>())
            {
                return entity.GetComponent<TagComponent>().Tag;
            }
            return entity.GetUUID().ToString();
        }
    }

    void ScriptWorld::OnRuntimeStart(Scene& scene)
    {
        if (m_IsRunning)
        {
            HE_CORE_WARN("Script runtime is already running");
            return;
        }

        auto scriptView = scene.GetRegistry().view<ScriptComponent, IDComponent>();
        for (const entt::entity entityHandle : scriptView)
        {
            Entity entity(entityHandle, &scene);
            const ScriptComponent& scriptComponent = scriptView.get<ScriptComponent>(entityHandle);

            for (uint32_t slotIndex = 0; slotIndex < static_cast<uint32_t>(scriptComponent.Scripts.size()); ++slotIndex)
            {
                const ScriptComponent::ScriptReference& script = scriptComponent.Scripts[slotIndex];
                if (script.Path.empty())
                {
                    HE_CORE_WARN("Entity '{}' has an empty script path in slot {}", GetEntityDisplayName(entity), slotIndex);
                    continue;
                }

                ScriptEngine* engine = ScriptManager::GetEngineForFile(script.Path);
                if (engine == nullptr)
                {
                    HE_CORE_WARN("No scripting backend registered for script '{}' on entity '{}'", script.Path, GetEntityDisplayName(entity));
                    continue;
                }

                ScriptRuntime* runtime = nullptr;
                for (RuntimeEntry& entry : m_Runtimes)
                {
                    if (entry.Engine == engine)
                    {
                        runtime = entry.Runtime.get();
                        break;
                    }
                }

                if (runtime == nullptr)
                {
                    Scope<ScriptRuntime> newRuntime = engine->CreateRuntime(scene);
                    if (newRuntime == nullptr)
                    {
                        HE_CORE_ERROR("{} scripting backend failed to create a runtime", engine->GetName());
                        continue;
                    }

                    RuntimeEntry& entry = m_Runtimes.emplace_back();
                    entry.Engine = engine;
                    entry.Runtime = std::move(newRuntime);
                    runtime = entry.Runtime.get();
                }

                if (!script.Enabled)
                {
                    HE_CORE_INFO("Skipped disabled script '{}' on entity '{}'", script.Path, GetEntityDisplayName(entity));
                    continue;
                }

                runtime->CreateInstance(entity, slotIndex, script.Path, script.Enabled);
            }
        }

        m_IsRunning = true;
    }

    void ScriptWorld::OnUpdate(Timestep timestep, Scene& scene)
    {
        if (!m_IsRunning)
        {
            return;
        }

        for (RuntimeEntry& entry : m_Runtimes)
        {
            entry.Runtime->Update(timestep, scene);
        }
    }

    void ScriptWorld::OnRuntimeStop(Scene& scene)
    {
        if (!m_IsRunning)
        {
            return;
        }

        for (RuntimeEntry& entry : m_Runtimes)
        {
            entry.Runtime->Shutdown(scene);
        }

        m_Runtimes.clear();
        m_IsRunning = false;
    }
}
