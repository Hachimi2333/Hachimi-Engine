#include "Scripting/Lua/LuaScriptRuntime.h"

#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Scene.h"
#include "Scripting/Lua/LuaScriptBindings.h"
#include "Scripting/Lua/LuaScriptRequire.h"
#include "Scripting/ScriptErrorLocation.h"
#include "Scripting/ScriptFields.h"
#include "Scripting/ScriptEntity.h"
#include "Utils/FileSystem.h"
#include "Scripting/ScriptManager.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/VirtualFileSystem.h"

#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace HachimiEngine
{
    namespace
    {
        struct LuaScriptInstance
        {
            Entity Entity;
            uint32_t SlotIndex = 0;
            std::string DisplayName;
            // Name the chunk was loaded under. It is what Lua puts in front of an error it raises.
            std::string ChunkName;
            // Captured at creation so a summary after the entity is gone can still name it.
            std::string EntityName;
            bool IsBroken = false;
            uint32_t ConsecutiveFailures = 0;
            std::filesystem::path SourcePath;
            std::filesystem::file_time_type LastWrite {};
            sol::environment Environment;
            sol::object Module = sol::nil;
            // The module's `fields` table. Values the script writes land here first and are copied
            // into the component after the callback returns.
            sol::table Fields;
            // Last value pushed into the table, per field name. Comparing against it is what tells a
            // value the script wrote apart from one edited in the inspector while the game runs.
            std::unordered_map<std::string, ScriptFieldData> PushedValues;
            // Fields already reported, so one wrong type is one console line instead of one per frame.
            std::unordered_set<std::string> ReportedFields;
            uint32_t ErrorCount = 0;
            // Formatted location of the last failure, kept for the summary printed at shutdown.
            std::string LastError;
        };

        std::string GetEntityDisplayName(Entity entity)
        {
            if (entity && entity.HasComponent<TagComponent>())
            {
                return entity.GetComponent<TagComponent>().Tag;
            }
            return "Unknown Entity";
        }

        // Lua truncates the chunk name it reports to sixty characters, so the absolute path an asset
        // resolves to would reach the console with its middle cut out. The path relative to the
        // project's Assets folder is short and is the one the author recognizes.
        std::string MakeChunkName(const std::filesystem::path& sourcePath, const std::filesystem::path& assetsDirectory,
                                  const std::string& displayName)
        {
            const std::string relativeText = sourcePath.lexically_relative(assetsDirectory).generic_string();
            if (!relativeText.empty() && relativeText.rfind("..", 0) != 0)
            {
                return "Assets/" + relativeText;
            }

            return displayName.empty() ? sourcePath.filename().string() : displayName;
        }

        // The script slot the instance was created from, or null when the component or the slot is
        // gone: both are reachable, because a script may delete its own entity.
        ScriptComponent::ScriptReference* GetReference(LuaScriptInstance& instance)
        {
            if (!instance.Entity || !instance.Entity.HasComponent<ScriptComponent>())
            {
                return nullptr;
            }

            auto& scripts = instance.Entity.GetComponent<ScriptComponent>().Scripts;
            return instance.SlotIndex < scripts.size() ? &scripts[instance.SlotIndex] : nullptr;
        }

        sol::object FieldToLua(sol::state& state, const ScriptFieldData& data)
        {
            return std::visit([&state](const auto& value)
            {
                return sol::make_object(state, value);
            }, data);
        }

        // Copies a value the script wrote back into the field's type. A value of the wrong type is
        // refused with a message instead of being converted: turning a string into a float behind
        // the script's back hides the mistake that produced it.
        bool FieldFromLua(const sol::object& value, const ScriptFieldData& current, ScriptFieldData& outData,
                          std::string& outError)
        {
            const ScriptFieldType type = ScriptFields::GetType(current);
            const std::string_view typeName = ScriptFields::GetTypeName(type);

            switch (type)
            {
                case ScriptFieldType::Bool:
                    if (value.get_type() != sol::type::boolean)
                    {
                        break;
                    }
                    outData = value.as<bool>();
                    return true;
                case ScriptFieldType::Int:
                {
                    if (value.get_type() != sol::type::number)
                    {
                        break;
                    }
                    const double number = value.as<double>();
                    if (number != std::floor(number))
                    {
                        outError = "expects a whole number";
                        return false;
                    }
                    outData = static_cast<int64_t>(number);
                    return true;
                }
                case ScriptFieldType::Float:
                    if (value.get_type() != sol::type::number)
                    {
                        break;
                    }
                    outData = value.as<float>();
                    return true;
                case ScriptFieldType::String:
                    if (value.get_type() != sol::type::string)
                    {
                        break;
                    }
                    outData = value.as<std::string>();
                    return true;
                case ScriptFieldType::Vec3:
                    if (!value.is<Math::Vec3>())
                    {
                        break;
                    }
                    outData = value.as<Math::Vec3>();
                    return true;
            }

            outError = "expects a '" + std::string(typeName) + "'";
            return false;
        }

        void ReportFieldIssue(LuaScriptInstance& instance, const std::string& fieldName, const std::string& detail)
        {
            if (!instance.ReportedFields.insert(fieldName).second)
            {
                return;
            }

            HE_CORE_WARN("Script '{}' on entity '{}': field '{}' {}", instance.DisplayName, instance.EntityName,
                fieldName, detail);
        }

        // Pushes field values into the script's table. Only values that changed since the last push
        // travel, so a value the script wrote itself is never overwritten with a stale copy.
        void PushFields(LuaScriptInstance& instance, ScriptComponent::ScriptReference& reference, sol::state& state)
        {
            for (const ScriptField& field : reference.Fields)
            {
                const auto pushed = instance.PushedValues.find(field.Name);
                if (pushed != instance.PushedValues.end() && pushed->second == field.Value)
                {
                    continue;
                }

                instance.Fields.set(field.Name, FieldToLua(state, field.Value));
                instance.PushedValues[field.Name] = field.Value;
            }
        }

        // Copies the table back into the component. Only the runtime copy is written, which is what
        // keeps a Play session from changing the design-time values the scene stores.
        void PullFields(LuaScriptInstance& instance, ScriptComponent::ScriptReference& reference)
        {
            for (ScriptField& field : reference.Fields)
            {
                const sol::object value = instance.Fields.get<sol::object>(field.Name);
                if (value.get_type() == sol::type::nil)
                {
                    continue;
                }

                ScriptFieldData data;
                std::string error;
                if (!FieldFromLua(value, field.Value, data, error))
                {
                    ReportFieldIssue(instance, field.Name, error + "; the written value is ignored");
                    continue;
                }

                if (data != field.Value)
                {
                    field.Value = data;
                }
                instance.PushedValues[field.Name] = data;
            }

            // A name the slot does not declare is reported once rather than silently kept: reading
            // it back never will, so a typo in the script would otherwise look like a working field.
            for (const auto& entry : instance.Fields)
            {
                if (entry.first.get_type() != sol::type::string)
                {
                    continue;
                }

                const std::string name = entry.first.as<std::string>();
                if (ScriptFields::Find(reference.Fields, name) == nullptr)
                {
                    ReportFieldIssue(instance, name, "is not declared on this script slot");
                }
            }
        }

        void CallLifecycle(LuaScriptInstance& instance, const std::string& functionName, std::optional<float> deltaTime,
                           bool allowBroken)
        {
            if (!instance.Entity && !allowBroken) return;
            instance.IsBroken = false;

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

                if (result.valid()) instance.ConsecutiveFailures = 0;
                if (!result.valid())
                {
                    const sol::error error = result;
                    // The backend is the only place that knows a script failed and which one it was,
                    // so the location is resolved here and travels as text from this point on.
                    const ScriptLocation location = ParseScriptError(error.what(), instance.ChunkName);
                    instance.IsBroken = true;
                    ++instance.ErrorCount;
                    ++instance.ConsecutiveFailures;
                    instance.LastError = FormatScriptLocation(location);

                    HE_CORE_ERROR("Lua error during {} on '{}' (entity '{}'): {}", functionName, instance.DisplayName,
                        instance.EntityName, instance.LastError);
                }
            }
            catch (const std::exception& exception)
            {
                instance.IsBroken = true;
                ++instance.ErrorCount;
                ++instance.ConsecutiveFailures;
                instance.LastError = FormatScriptLocation(ParseScriptError(exception.what(), instance.ChunkName));

                HE_CORE_ERROR("Lua exception during {} on '{}' (entity '{}'): {}", functionName, instance.DisplayName,
                    instance.EntityName, instance.LastError);
            }
        }
    }

    struct LuaScriptRuntime::Impl
    {
        Scene* Scene = nullptr;
        // Lua values are destroyed before State, including the cache and instance environments.
        sol::state State;
        LuaModuleCache ModuleCache;
        sol::table TimeTable;
        std::filesystem::path AssetsDirectory;
        std::filesystem::path ScriptRoot;
        std::vector<Scope<LuaScriptInstance>> Instances;
        float ReloadElapsed = 0.0f;

        void RefreshTime(float delta)
        {
            const Time& clock = Scene->GetTime();
            TimeTable["DeltaTime"] = delta;
            TimeTable["UnscaledDeltaTime"] = clock.GetUnscaledDeltaTime();
            TimeTable["FixedDeltaTime"] = Scene->GetPhysicsSettings().FixedTimeStep;
            TimeTable["TimeScale"] = clock.GetTimeScale();
            TimeTable["ElapsedTime"] = clock.GetElapsedTime();
            TimeTable["FrameCount"] = clock.GetFrameCount();
        }

        void ReadTimeScale()
        {
            sol::object value = TimeTable["TimeScale"];
            if (value.is<float>()) Scene->GetTime().SetTimeScale(value.as<float>());
        }

        bool LoadModule(LuaScriptInstance& instance, sol::object& outModule)
        {
            std::string source;
            if (!VirtualFileSystem::ReadTextFile(instance.SourcePath, source))
            {
                HE_CORE_ERROR("Failed to read Lua script '{}'", instance.SourcePath.string());
                return false;
            }
            const sol::protected_function_result result = State.safe_script(source, instance.Environment,
                sol::script_pass_on_error, instance.ChunkName);
            if (!result.valid())
            {
                const sol::error error = result;
                instance.LastError = FormatScriptLocation(ParseScriptError(error.what(), instance.ChunkName));
                ++instance.ErrorCount;
                HE_CORE_ERROR("Failed to load Lua script '{}': {}", instance.DisplayName, instance.LastError);
                return false;
            }
            if (result.return_count() == 0 || result.get<sol::object>(0).get_type() != sol::type::table)
            {
                HE_CORE_ERROR("Lua script '{}' must return a module table", instance.DisplayName);
                return false;
            }
            outModule = result.get<sol::object>(0);
            return true;
        }

        void BindFields(LuaScriptInstance& instance, bool resetDefaults)
        {
            instance.Fields = State.create_table();
            instance.PushedValues.clear();
            instance.ReportedFields.clear();
            if (auto* reference = GetReference(instance))
            {
                for (auto& field : reference->Fields)
                {
                    if (resetDefaults) field.Value = field.DefaultValue;
                }
                PushFields(instance, *reference, State);
            }
            sol::table module = instance.Module.as<sol::table>();
            AttachLuaScriptInstance(module, instance.Entity, *Scene, State, instance.Fields);
        }

        void RunCallback(const char* name, float delta)
        {
            RefreshTime(delta);
            std::erase_if(Instances, [](const auto& instance)
            {
                if (GetReference(*instance) != nullptr) return false;
                CallLifecycle(*instance, "OnDestroy", std::nullopt, true);
                return true;
            });
            for (const auto& instance : Instances)
            {
                auto* reference = GetReference(*instance);
                if (reference == nullptr || !reference->Enabled || instance->Module.get_type() != sol::type::table) continue;
                PushFields(*instance, *reference, State);
                CallLifecycle(*instance, name, delta, false);
                if (auto* current = GetReference(*instance)) PullFields(*instance, *current);
            }
            ReadTimeScale();
        }
    };

    LuaScriptRuntime::LuaScriptRuntime(Scene& scene)
        : m_Impl(CreateScope<Impl>())
    {
        m_Impl->Scene = &scene;
        m_Impl->State.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
            sol::lib::table, sol::lib::coroutine, sol::lib::utf8);
        m_Impl->State["dofile"] = sol::nil;
        m_Impl->State["loadfile"] = sol::nil;
        const AssetDatabase* database = SceneSerializer::GetAssetDatabase();
        m_Impl->AssetsDirectory = database != nullptr ? database->GetAssetsDirectory() : std::filesystem::path("Assets");
        m_Impl->ScriptRoot = m_Impl->AssetsDirectory / "Scripts";
        m_Impl->TimeTable = m_Impl->State.create_table();
        m_Impl->RefreshTime(0.0f);
        RegisterLuaBindings(m_Impl->State, scene, m_Impl->TimeTable);
        RegisterLuaRequire(m_Impl->State, m_Impl->ScriptRoot, m_Impl->ModuleCache);
    }

    LuaScriptRuntime::~LuaScriptRuntime() = default;

    void LuaScriptRuntime::CreateInstance(Entity entity, uint32_t slotIndex, const std::filesystem::path& sourcePath,
        const std::string& displayName)
    {
        auto instance = CreateScope<LuaScriptInstance>();
        instance->Entity = entity;
        instance->SlotIndex = slotIndex;
        instance->DisplayName = displayName;
        instance->EntityName = GetEntityDisplayName(entity);
        instance->ChunkName = MakeChunkName(sourcePath, m_Impl->AssetsDirectory, displayName);
        instance->SourcePath = sourcePath;
        instance->Module = sol::make_object(m_Impl->State, sol::nil);
        instance->Fields = m_Impl->State.create_table();
        instance->Environment = sol::environment(m_Impl->State, sol::create, m_Impl->State.globals());
        if (FileSystem::IsFile(sourcePath)) instance->LastWrite = FileSystem::GetLastWriteTime(sourcePath);
        // Failed loads remain watched, so fixing the file can recover without restarting Play.
        LuaScriptInstance& live = *instance;
        m_Impl->Instances.push_back(std::move(instance));
        try
        {
            if (!m_Impl->LoadModule(live, live.Module)) return;
            m_Impl->BindFields(live, true);
            CallLifecycle(live, "OnCreate", std::nullopt, false);
            if (auto* reference = GetReference(live)) PullFields(live, *reference);
        }
        catch (const std::exception& error)
        {
            HE_CORE_ERROR("Failed to create Lua instance '{}': {}", displayName, error.what());
        }
    }

    void LuaScriptRuntime::Update(Timestep timestep, Scene& scene)
    {
        (void)scene;
        m_Impl->RunCallback("OnUpdate", timestep.GetSeconds());
    }

    void LuaScriptRuntime::FixedUpdate(Timestep timestep, Scene& scene)
    {
        (void)scene;
        m_Impl->RunCallback("FixedUpdate", timestep.GetSeconds());
    }

    void LuaScriptRuntime::OnPhysicsEvent(const ScriptPhysicsEvent& event, Scene& scene)
    {
        Entity a = scene.GetEntityByKey(event.EntityKeyA);
        Entity b = scene.GetEntityByKey(event.EntityKeyB);
        if (!a || !b) return;
        const char* name = event.Type == ScriptPhysicsEvent::Kind::CollisionEnter ? "OnCollisionEnter"
            : event.Type == ScriptPhysicsEvent::Kind::CollisionExit ? "OnCollisionExit"
            : event.Type == ScriptPhysicsEvent::Kind::TriggerEnter ? "OnTriggerEnter" : "OnTriggerExit";
        m_Impl->RefreshTime(scene.GetTime().GetDeltaTime());
        for (const auto& instance : m_Impl->Instances)
        {
            auto* reference = GetReference(*instance);
            if (reference == nullptr || !reference->Enabled || instance->Module.get_type() != sol::type::table) continue;
            if (instance->Entity != a && instance->Entity != b) continue;
            PushFields(*instance, *reference, m_Impl->State);
            sol::table module = instance->Module.as<sol::table>();
            sol::object callback = module[name];
            if (callback.get_type() != sol::type::function) continue;
            const bool recipientA = instance->Entity == a;
            ScriptEntity other(recipientA ? b : a, &scene);
            sol::protected_function function = callback.as<sol::protected_function>();
            sol::table contact = m_Impl->State.create_table();
            contact["point"] = event.Point;
            contact["normal"] = recipientA ? event.Normal : -event.Normal;
            contact["impulse"] = event.Impulse;
            const sol::protected_function_result result = event.Type == ScriptPhysicsEvent::Kind::CollisionEnter
                ? function(module, other, contact) : function(module, other);
            instance->IsBroken = !result.valid();
            if (!result.valid())
            {
                const sol::error error = result;
                ++instance->ErrorCount;
                ++instance->ConsecutiveFailures;
                instance->LastError = FormatScriptLocation(ParseScriptError(error.what(), instance->ChunkName));
                HE_CORE_ERROR("Lua error during {} on '{}': {}", name, instance->DisplayName, instance->LastError);
            }
            else instance->ConsecutiveFailures = 0;
            if (auto* current = GetReference(*instance)) PullFields(*instance, *current);
        }
        m_Impl->ReadTimeScale();
    }

    void LuaScriptRuntime::PollHotReload(float unscaledDelta, Scene& scene)
    {
        (void)scene;
        m_Impl->ReloadElapsed += unscaledDelta;
        if (m_Impl->ReloadElapsed < 0.5f) return;
        m_Impl->ReloadElapsed = 0.0f;
        bool dependencyChanged = false;
        for (const auto& [path, stamp] : m_Impl->ModuleCache.WriteTimes)
            if (FileSystem::IsFile(path) && FileSystem::GetLastWriteTime(path) != stamp)
                dependencyChanged = true;
        if (dependencyChanged) m_Impl->ModuleCache.Clear();
        for (const auto& instance : m_Impl->Instances)
        {
            if (GetReference(*instance) == nullptr || !FileSystem::IsFile(instance->SourcePath)) continue;
            const auto stamp = FileSystem::GetLastWriteTime(instance->SourcePath);
            if (!dependencyChanged && stamp == instance->LastWrite) continue;
            instance->LastWrite = stamp;
            try
            {
                // Reuse the instance environment. Restore its entries if evaluation fails so the
                // old module remains usable after a syntax/runtime error during loading.
                sol::table snapshot = m_Impl->State.create_table();
                for (const auto& entry : instance->Environment) snapshot.set(entry.first, entry.second);
                sol::object replacement;
                if (!m_Impl->LoadModule(*instance, replacement))
                {
                    std::vector<sol::object> keys;
                    for (const auto& entry : instance->Environment) keys.push_back(entry.first);
                    for (const auto& key : keys) instance->Environment.set(key, sol::nil);
                    for (const auto& entry : snapshot) instance->Environment.set(entry.first, entry.second);
                    continue;
                }
                if (instance->Module.get_type() == sol::type::table)
                {
                    if (auto* reference = GetReference(*instance)) PushFields(*instance, *reference, m_Impl->State);
                    CallLifecycle(*instance, "OnDestroy", std::nullopt, true);
                    if (auto* reference = GetReference(*instance)) PullFields(*instance, *reference);
                }
                instance->Module = replacement;
                instance->IsBroken = false;
                instance->ConsecutiveFailures = 0;
                m_Impl->BindFields(*instance, false);
                CallLifecycle(*instance, "OnCreate", std::nullopt, false);
                CallLifecycle(*instance, "OnReload", std::nullopt, false);
                if (auto* reference = GetReference(*instance)) PullFields(*instance, *reference);
                HE_CORE_INFO("Reloaded Lua script '{}'", instance->ChunkName);
            }
            catch (const std::exception& error)
            {
                HE_CORE_ERROR("Failed to reload '{}': {}", instance->DisplayName, error.what());
            }
        }
    }

    void LuaScriptRuntime::Shutdown(Scene& scene)
    {
        (void)scene;
        for (const auto& instance : m_Impl->Instances)
        {
            CallLifecycle(*instance, "OnDestroy", std::nullopt, true);
            if (instance->ErrorCount != 0)
                HE_CORE_ERROR("Script '{}' on entity '{}' failed {} time(s); last error: {}",
                    instance->DisplayName, instance->EntityName, instance->ErrorCount, instance->LastError);
        }
        m_Impl->Instances.clear();
        m_Impl->ModuleCache.Clear();
        m_Impl->State.collect_garbage();
    }
}
