#include "Scene/Components/ScriptComponent.h"

#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "Scene/Components/AssetHandleSerialization.h"
#include "Serialization/SceneSerializer.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "ScriptComponent";

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            // Guarded rather than overwriting, and one empty slot so a freshly added component has
            // something to point at without the inspector special-casing an empty list.
            if (!registry.all_of<ScriptComponent>(entity))
            {
                registry.emplace<ScriptComponent>(entity).Scripts.emplace_back();
            }
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<ScriptComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<ScriptComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<ScriptComponent>(sourceEntity))
            {
                target.emplace_or_replace<ScriptComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<ScriptComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;
            for (const ScriptComponent::ScriptReference& reference : component->Scripts)
            {
                out << YAML::BeginMap;
                AssetHandles::Emit(out, "Script", reference.Script);
                out << YAML::Key << "Path" << YAML::Value << reference.DisplayName;
                out << YAML::Key << "Enabled" << YAML::Value << reference.Enabled;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<ScriptComponent>(entity);
            component.Scripts.clear();

            // A const pointer is enough for every lookup; the one call that can register a path
            // goes through the non-const pointer the serializer owns.
            const AssetDatabase* database = SceneSerializer::GetAssetDatabase();
            AssetDatabase* mutableDatabase = SceneSerializer::GetAssetDatabase();
            if (const YAML::Node scriptsNode = node["Scripts"]; scriptsNode && scriptsNode.IsSequence())
            {
                for (const YAML::Node referenceNode : scriptsNode)
                {
                    ScriptComponent::ScriptReference reference;
                    reference.DisplayName = referenceNode["Path"].as<std::string>("");
                    reference.Enabled = referenceNode["Enabled"].as<bool>(true);
                    reference.Script = AssetHandles::Read(referenceNode["Script"], AssetType::Script);

                    if (reference.Script.IsValid())
                    {
                        // The name that shipped with the reference is a label, not the truth: the
                        // file may have been renamed since, so the database wins when it knows.
                        if (database != nullptr && database->Contains(reference.Script))
                        {
                            reference.DisplayName = database->GetDisplayName(reference.Script);
                        }
                    }
                    else if (!reference.DisplayName.empty() && mutableDatabase != nullptr)
                    {
                        // Format version 2 stored only a path relative to Assets/Scripts. Resolve it
                        // once, here, so nothing downstream has to understand the old shape.
                        const std::filesystem::path scriptsDirectory = mutableDatabase->GetAssetsDirectory() / "Scripts";
                        reference.Script = mutableDatabase->EnsureAsset(
                            scriptsDirectory / std::filesystem::path(reference.DisplayName),
                            AssetType::Script).Handle;

                        if (!reference.Script.IsValid())
                        {
                            HE_CORE_WARN("Script '{}' is not in the project; the reference is kept as missing",
                                reference.DisplayName);
                        }
                    }

                    component.Scripts.push_back(std::move(reference));
                }
            }

            return true;
        }
    }

    ComponentDescriptor MakeScriptComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Script";
        descriptor.TypeID = entt::type_hash<ScriptComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
