#include "Serialization/SceneSerializer.h"

#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* FormatVersionKey = "FormatVersion";
        constexpr const char* SceneKey = "Scene";
        constexpr const char* EnvironmentKey = "Environment";
        constexpr const char* PhysicsKey = "Physics";
        constexpr const char* EntitiesKey = "Entities";

        // Component blocks this build does not know about, kept verbatim so that saving a scene
        // written by a newer build does not silently delete data.
        //
        // The type is deliberately not registered: the generic machinery must not create,
        // duplicate or clone it, because only the serializer can interpret the payload.
        struct UnknownComponentBlocks
        {
            std::vector<std::pair<std::string, YAML::Node>> Blocks;
        };

        void EmitVec3(YAML::Emitter& out, const Math::Vec3& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
        }

        Math::Vec3 ReadVec3(const YAML::Node& node, const Math::Vec3& fallback = Math::Vec3(0.0f))
        {
            if (!node || !node.IsSequence() || node.size() < 3)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
        }

        // Keys that are structure rather than a component block.
        const std::unordered_set<std::string>& ReservedKeys()
        {
            static const std::unordered_set<std::string> keys { "Entity", EntitiesKey, "UnknownComponents" };
            return keys;
        }

        const ComponentDescriptor* FindDescriptorByName(std::string_view name)
        {
            for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
            {
                if (descriptor.Name == name)
                {
                    return &descriptor;
                }
            }
            return nullptr;
        }
    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
    }

    AssetDatabase* SceneSerializer::s_AssetDatabase = nullptr;

    bool SceneSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << FormatVersionKey << YAML::Value << CurrentFormatVersion;
        out << YAML::Key << SceneKey << YAML::Value << m_Scene->GetName();

        const EnvironmentSettings& environment = m_Scene->GetEnvironmentSettings();
        out << YAML::Key << EnvironmentKey << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "ShowSkybox" << YAML::Value << environment.ShowSkybox;
        out << YAML::Key << "Exposure" << YAML::Value << environment.Exposure;
        out << YAML::Key << "EnvironmentIntensity" << YAML::Value << environment.EnvironmentIntensity;
        out << YAML::EndMap;

        const PhysicsSettings& physics = m_Scene->GetPhysicsSettings();
        out << YAML::Key << PhysicsKey << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Gravity" << YAML::Value;
        EmitVec3(out, physics.Gravity);
        out << YAML::Key << "FixedTimeStep" << YAML::Value << physics.FixedTimeStep;
        out << YAML::Key << "SubStepCount" << YAML::Value << physics.SubStepCount;
        out << YAML::Key << "EnableSleep" << YAML::Value << physics.EnableSleep;
        out << YAML::Key << "EnableContinuous" << YAML::Value << physics.EnableContinuous;
        out << YAML::EndMap;

        out << YAML::Key << EntitiesKey << YAML::Value << YAML::BeginSeq;
        for (const Entity entity : m_Scene->GetAllEntities())
        {
            SerializeEntity(out, entity);
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        if (!file)
        {
            HE_CORE_ERROR("Cannot open scene file for writing: {}", filepath);
            return false;
        }

        file << out.c_str();
        if (!file)
        {
            HE_CORE_ERROR("Failed to write scene file: {}", filepath);
            return false;
        }

        // The file on disk now matches the scene, so the editor's unsaved-changes prompt clears.
        m_Scene->ClearDirty();
        return true;
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        // Read through the virtual file system so packaged scenes load straight out of the
        // game package.
        std::string sceneText;
        if (!VirtualFileSystem::ReadTextFile(filepath, sceneText))
        {
            HE_CORE_ERROR("Failed to read scene file: {}", filepath);
            return false;
        }

        YAML::Node data;
        try
        {
            data = YAML::Load(sceneText);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Failed to parse scene file '{}': {}", filepath, exception.what());
            return false;
        }

        if (!data || !data[SceneKey])
        {
            HE_CORE_ERROR("Failed to load scene file: {}", filepath);
            return false;
        }

        const int formatVersion = data[FormatVersionKey].as<int>(0);
        if (formatVersion != CurrentFormatVersion)
        {
            // Refused rather than guessed at. Component references changed shape (materials and
            // scripts are asset UUIDs now), so reading an older file as if it had this layout would
            // silently drop those references.
            HE_CORE_ERROR("Scene '{}' uses format version {}; this build reads version {}. "
                          "Older scenes are not migrated: recreate the project or re-save the scene from a build that wrote it.",
                filepath,
                formatVersion,
                CurrentFormatVersion);
            return false;
        }

        m_Scene->OnRuntimeStop();
        m_Scene->m_EntityMap.clear();
        m_Scene->m_ChildrenIndex.clear();
        m_Scene->m_ChildrenIndexDirty = true;
        m_Scene->m_Registry.clear();
        m_Scene->SetName(data[SceneKey].as<std::string>());

        if (const YAML::Node environmentNode = data[EnvironmentKey])
        {
            EnvironmentSettings& environment = m_Scene->GetEnvironmentSettings();
            environment.ShowSkybox = environmentNode["ShowSkybox"].as<bool>(true);
            environment.Exposure = environmentNode["Exposure"].as<float>(1.0f);
            environment.EnvironmentIntensity = environmentNode["EnvironmentIntensity"].as<float>(1.0f);
        }

        if (const YAML::Node physicsNode = data[PhysicsKey])
        {
            PhysicsSettings& physics = m_Scene->GetPhysicsSettings();
            physics.Gravity = ReadVec3(physicsNode["Gravity"], physics.Gravity);
            physics.FixedTimeStep = physicsNode["FixedTimeStep"].as<float>(physics.FixedTimeStep);
            physics.SubStepCount = physicsNode["SubStepCount"].as<int>(physics.SubStepCount);
            physics.EnableSleep = physicsNode["EnableSleep"].as<bool>(physics.EnableSleep);
            physics.EnableContinuous = physicsNode["EnableContinuous"].as<bool>(physics.EnableContinuous);
        }

        const YAML::Node entities = data[EntitiesKey];
        if (entities && entities.IsSequence())
        {
            for (const YAML::Node entityNode : entities)
            {
                DeserializeEntity(entityNode, *m_Scene);
            }
        }

        // Loading a scene is not an edit of it.
        m_Scene->ClearDirty();
        return true;
    }

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        const Scene& scene = *m_Scene;
        SerializeEntity(out, scene, entity);
    }

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, const Scene& scene, Entity entity)
    {
        out << YAML::BeginMap;

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            descriptor.Serialize(out, scene.GetRegistry(), entity.GetHandle());
        }

        // Anything this build did not recognise is written back unchanged.
        if (const auto* unknownBlocks = scene.GetRegistry().try_get<UnknownComponentBlocks>(entity.GetHandle()))
        {
            for (const auto& [name, node] : unknownBlocks->Blocks)
            {
                out << YAML::Key << name << YAML::Value << node;
            }
        }

        out << YAML::EndMap;
    }

    std::string SceneSerializer::SerializeEntitiesToString(const Scene& scene, const std::vector<Entity>& entities)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << FormatVersionKey << YAML::Value << CurrentFormatVersion;
        out << YAML::Key << EntitiesKey << YAML::Value << YAML::BeginSeq;

        for (const Entity entity : entities)
        {
            if (entity)
            {
                SerializeEntity(out, scene, entity);
            }
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
        return out.c_str();
    }

    size_t SceneSerializer::DeserializeEntitiesFromString(Scene& scene, const std::string& text)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(text);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Stored entity snapshot is not valid YAML: {}", exception.what());
            return 0;
        }

        const YAML::Node entities = data[EntitiesKey];
        if (!entities || !entities.IsSequence())
        {
            return 0;
        }

        size_t created = 0;
        SceneSerializer helper(nullptr);

        for (const YAML::Node entityNode : entities)
        {
            const size_t before = scene.GetRegistry().view<IDComponent>().size();
            helper.DeserializeEntity(entityNode, scene);
            if (scene.GetRegistry().view<IDComponent>().size() > before)
            {
                ++created;
            }
        }
        return created;
    }

    std::string SceneSerializer::SerializeComponentToString(const Scene& scene, Entity entity, entt::id_type typeId)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(typeId);
        if (descriptor == nullptr || !entity)
        {
            return {};
        }

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << FormatVersionKey << YAML::Value << CurrentFormatVersion;
        out << YAML::Key << EntitiesKey << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, scene, entity);
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return out.c_str();
    }

    bool SceneSerializer::DeserializeComponentFromString(Scene& scene, Entity entity, const std::string& text)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(text);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Stored component snapshot is not valid YAML: {}", exception.what());
            return false;
        }

        const YAML::Node entities = data[EntitiesKey];
        if (!entities || !entities.IsSequence() || entities.size() == 0)
        {
            return false;
        }

        // The snapshot holds one entity, and only its component values are wanted: the entity the
        // caller is restoring into keeps its own identity, hierarchy and other components.
        bool restored = false;
        const YAML::Node entityNode = entities[0];
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.Required)
            {
                // Required components are already there and must not be touched.
                continue;
            }

            if (descriptor.Deserialize(entityNode, scene.GetRegistry(), entity.GetHandle()))
            {
                restored = true;
            }
        }

        return restored;
    }

    void SceneSerializer::DeserializeEntity(const YAML::Node& entityNode, Scene& scene)
    {
        const entt::entity handle = scene.m_Registry.create();

        bool hasIdentity = false;
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            const bool read = descriptor.Deserialize(entityNode, scene.m_Registry, handle);
            if (read && descriptor.TypeID == entt::type_hash<IDComponent>::value())
            {
                hasIdentity = true;
            }
            else if (!read && descriptor.Required)
            {
                // A required component that is absent from the file still exists on the entity,
                // so the rest of the engine can keep assuming it is there.
                descriptor.AddDefault(scene.m_Registry, handle);
            }
        }

        if (!hasIdentity)
        {
            HE_CORE_ERROR("Entity without a readable UUID in the scene file is skipped");
            scene.m_Registry.destroy(handle);
            return;
        }

        std::vector<std::pair<std::string, YAML::Node>> unknownBlocks;
        for (const auto& entry : entityNode)
        {
            if (!entry.first.IsScalar())
            {
                continue;
            }

            const std::string name = entry.first.as<std::string>();
            if (ReservedKeys().contains(name) || FindDescriptorByName(name) != nullptr)
            {
                continue;
            }

            HE_CORE_WARN("Scene contains a component block this build does not know: '{}'", name);
            unknownBlocks.emplace_back(name, entry.second);
        }

        if (!unknownBlocks.empty())
        {
            scene.m_Registry.emplace<UnknownComponentBlocks>(handle, UnknownComponentBlocks { std::move(unknownBlocks) });
        }

        scene.m_EntityMap[scene.m_Registry.get<IDComponent>(handle).ID] = handle;
        scene.m_ChildrenIndexDirty = true;
    }
}
