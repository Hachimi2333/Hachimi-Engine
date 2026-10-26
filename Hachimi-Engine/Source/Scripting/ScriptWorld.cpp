#include "Scripting/ScriptWorld.h"

#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptRuntime.h"
#include "Serialization/SceneSerializer.h"

#include <filesystem>

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

        scene.GetDispatcher().sink<SceneCollisionEvent>().connect<&ScriptWorld::OnCollision>(*this);
        scene.GetDispatcher().sink<SceneTriggerEvent>().connect<&ScriptWorld::OnTrigger>(*this);

        // The database owns the mapping from a script reference to its file; a runtime only needs
        // the path it resolved to.
        const AssetDatabase* database = SceneSerializer::GetAssetDatabase();

        auto scriptView = scene.GetRegistry().view<ScriptComponent, IDComponent>();
        for (const entt::entity entityHandle : scriptView)
        {
            Entity entity(entityHandle, &scene);
            const ScriptComponent& scriptComponent = scriptView.get<ScriptComponent>(entityHandle);

            for (uint32_t slotIndex = 0; slotIndex < static_cast<uint32_t>(scriptComponent.Scripts.size()); ++slotIndex)
            {
                const ScriptComponent::ScriptReference& script = scriptComponent.Scripts[slotIndex];

                if (!script.Enabled)
                {
                    HE_CORE_INFO("Skipped disabled script '{}' on entity '{}'", script.DisplayName, GetEntityDisplayName(entity));
                    continue;
                }

                std::filesystem::path sourcePath;
                if (script.Script.IsValid() && database != nullptr)
                {
                    sourcePath = database->GetAssetPath(script.Script);
                }

                if (sourcePath.empty())
                {
                    // A missing script is reported once per slot instead of silently producing an
                    // entity that looks scripted but never runs.
                    HE_CORE_ERROR("Script '{}' on entity '{}' is missing from the project; slot {} is not running",
                        script.DisplayName.empty() ? "<unnamed>" : script.DisplayName,
                        GetEntityDisplayName(entity),
                        slotIndex);
                    continue;
                }

                ScriptEngine* engine = ScriptManager::GetEngineForFile(sourcePath.string());
                if (engine == nullptr)
                {
                    HE_CORE_WARN("No scripting backend registered for script '{}' on entity '{}'",
                        sourcePath.string(),
                        GetEntityDisplayName(entity));
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

                const std::string displayName = script.DisplayName.empty()
                    ? sourcePath.stem().string()
                    : script.DisplayName;
                runtime->CreateInstance(entity, slotIndex, sourcePath, displayName);
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
            entry.Runtime->PollHotReload(scene.GetTime().GetUnscaledDeltaTime(), scene);
        for (const auto& event : m_Events)
            for (RuntimeEntry& entry : m_Runtimes)
                entry.Runtime->OnPhysicsEvent(event, scene);
        m_Events.clear();
        for (RuntimeEntry& entry : m_Runtimes)
            entry.Runtime->Update(timestep, scene);
    }

    void ScriptWorld::OnFixedUpdate(Timestep timestep, Scene& scene)
    {
        if (!m_IsRunning) return;
        for (RuntimeEntry& entry : m_Runtimes) entry.Runtime->FixedUpdate(timestep, scene);
    }

    void ScriptWorld::OnCollision(const SceneCollisionEvent& event)
    {
        for (const auto& begin : event.Begins)
            m_Events.push_back({ ScriptPhysicsEvent::Kind::CollisionEnter, begin.EntityKeyA, begin.EntityKeyB, begin.Point, begin.Normal, begin.Impulse });
        for (const auto& end : event.Ends)
            m_Events.push_back({ ScriptPhysicsEvent::Kind::CollisionExit, end.EntityKeyA, end.EntityKeyB });
    }

    void ScriptWorld::OnTrigger(const SceneTriggerEvent& event)
    {
        for (const auto& begin : event.Begins)
            m_Events.push_back({ ScriptPhysicsEvent::Kind::TriggerEnter, begin.SensorEntityKey, begin.OtherEntityKey });
        for (const auto& end : event.Ends)
            m_Events.push_back({ ScriptPhysicsEvent::Kind::TriggerExit, end.SensorEntityKey, end.OtherEntityKey });
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

        scene.GetDispatcher().sink<SceneCollisionEvent>().disconnect<&ScriptWorld::OnCollision>(*this);
        scene.GetDispatcher().sink<SceneTriggerEvent>().disconnect<&ScriptWorld::OnTrigger>(*this);
        m_Events.clear();
        m_Runtimes.clear();
        m_IsRunning = false;
    }
}
