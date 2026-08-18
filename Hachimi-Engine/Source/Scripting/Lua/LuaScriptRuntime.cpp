#include "Scripting/Lua/LuaScriptRuntime.h"

#include "Core/Log.h"
#include "Scene/Scene.h"
#include "Scripting/Lua/LuaScriptBindings.h"
#include "Scripting/ScriptManager.h"
#include "Utils/FileSystem.h"

#include <optional>

namespace HachimiEngine
{
    namespace
    {
        struct LuaScriptInstance
        {
            Entity Entity;
            uint32_t SlotIndex = 0;
            std::string RelativePath;
            bool IsBroken = false;
            sol::object Module = sol::nil;
        };

        std::string GetEntityDisplayName(const LuaScriptInstance& instance)
        {
            if (instance.Entity && instance.Entity.HasComponent<TagComponent>())
            {
                return instance.Entity.GetComponent<TagComponent>().Tag;
            }
            return "Unknown Entity";
        }

        void CallLifecycle(LuaScriptInstance& instance, const std::string& functionName, std::optional<float> deltaTime, bool allowBroken)
        {
            if (instance.IsBroken && !allowBroken)
            {
                return;
            }

            if (instance.Module.get_type() != sol::type::table)
            {
                return;
            }

            try
            {
                const sol::table module = instance.Module.as<sol::table>();
                const sol::object functionObject = module[functionName];
                if (functionObject.get_type() != sol::type::function)
                {
                    return;
                }

                const sol::protected_function function = functionObject.as<sol::protected_function>();
                const sol::protected_function_result result = deltaTime.has_value()
                    ? function(module, deltaTime.value())
                    : function(module);

                if (!result.valid())
                {
                    const sol::error error = result;
                    instance.IsBroken = true;
                    HE_CORE_ERROR("Lua script error in '{}' during {} (entity '{}'): {}", instance.RelativePath, functionName, GetEntityDisplayName(instance), error.what());
                }
            }
            catch (const std::exception& exception)
            {
                instance.IsBroken = true;
                HE_CORE_ERROR("Lua script exception in '{}' during {} (entity '{}'): {}", instance.RelativePath, functionName, GetEntityDisplayName(instance), exception.what());
            }
        }
    }

    struct LuaScriptRuntime::Impl
    {
        Scene* Scene = nullptr;
        sol::state State;
        sol::table TimeTable;
        std::vector<Scope<LuaScriptInstance>> Instances;
        float ElapsedTime = 0.0f;
    };

    LuaScriptRuntime::LuaScriptRuntime(Scene& scene)
        : m_Impl(CreateScope<Impl>())
    {
        m_Impl->Scene = &scene;

        // Sandbox: core language libraries only. io, os, package, and debug are
        // intentionally unavailable to game scripts.
        m_Impl->State.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::coroutine,
            sol::lib::utf8);

        m_Impl->TimeTable = m_Impl->State.create_table();
        m_Impl->TimeTable["DeltaTime"] = 0.0f;
        m_Impl->TimeTable["ElapsedTime"] = 0.0f;

        RegisterLuaBindings(m_Impl->State, scene, m_Impl->TimeTable);
    }

    LuaScriptRuntime::~LuaScriptRuntime() = default;

    void LuaScriptRuntime::CreateInstance(Entity entity, uint32_t slotIndex, const std::string& relativePath, bool enabled)
    {
        if (!enabled)
        {
            return;
        }

        const std::filesystem::path fullPath = ScriptManager::ResolveScriptPath(relativePath);
        if (!FileSystem::Exists(fullPath))
        {
            HE_CORE_ERROR("Lua script file does not exist: {} (entity '{}')", fullPath.string(), entity.HasComponent<TagComponent>() ? entity.GetComponent<TagComponent>().Tag : "Unknown Entity");
            return;
        }

        Scope<LuaScriptInstance> instance = CreateScope<LuaScriptInstance>();
        instance->Entity = entity;
        instance->SlotIndex = slotIndex;
        instance->RelativePath = relativePath;

        try
        {
            const sol::environment environment(m_Impl->State, sol::create, m_Impl->State.globals());
            const sol::protected_function_result loadResult = m_Impl->State.safe_script_file(fullPath.string(), environment, sol::script_pass_on_error);

            if (!loadResult.valid())
            {
                const sol::error error = loadResult;
                HE_CORE_ERROR("Failed to load Lua script '{}': {}", relativePath, error.what());
                return;
            }

            if (loadResult.return_count() <= 0)
            {
                HE_CORE_ERROR("Lua script '{}' must return a module table", relativePath);
                return;
            }

            const sol::object moduleObject = loadResult.get<sol::object>(0);
            if (moduleObject.get_type() != sol::type::table)
            {
                HE_CORE_ERROR("Lua script '{}' returned a non-table value instead of a module table", relativePath);
                return;
            }

            sol::table module = moduleObject.as<sol::table>();
            AttachLuaScriptInstance(module, entity, *m_Impl->Scene);
            module["scene"] = m_Impl->State["HE"]["Scene"];

            instance->Module = moduleObject;
            m_Impl->Instances.push_back(std::move(instance));

            CallLifecycle(*m_Impl->Instances.back(), "OnCreate", std::nullopt, false);
        }
        catch (const sol::error& error)
        {
            HE_CORE_ERROR("Failed to create Lua script instance '{}': {}", relativePath, error.what());
        }
        catch (const std::exception& exception)
        {
            HE_CORE_ERROR("Failed to create Lua script instance '{}': {}", relativePath, exception.what());
        }
    }

    void LuaScriptRuntime::Update(Timestep timestep, Scene& scene)
    {
        m_Impl->ElapsedTime += timestep.GetSeconds();
        m_Impl->TimeTable["DeltaTime"] = timestep.GetSeconds();
        m_Impl->TimeTable["ElapsedTime"] = m_Impl->ElapsedTime;

        for (const Scope<LuaScriptInstance>& instance : m_Impl->Instances)
        {
            CallLifecycle(*instance, "OnUpdate", timestep.GetSeconds(), false);
        }
    }

    void LuaScriptRuntime::Shutdown(Scene& scene)
    {
        for (const Scope<LuaScriptInstance>& instance : m_Impl->Instances)
        {
            CallLifecycle(*instance, "OnDestroy", std::nullopt, true);
        }

        m_Impl->Instances.clear();
        m_Impl->State.collect_garbage();
    }
}
