#include "Scene/Components/ScriptComponent.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "ScriptComponent";

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            // One empty slot, ready to be pointed at a script file.
            registry.emplace<ScriptComponent>(entity).Scripts.emplace_back();
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
                out << YAML::Key << "Path" << YAML::Value << reference.Path;
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

            if (const YAML::Node scriptsNode = node["Scripts"]; scriptsNode && scriptsNode.IsSequence())
            {
                for (const YAML::Node referenceNode : scriptsNode)
                {
                    ScriptComponent::ScriptReference reference;
                    reference.Path = referenceNode["Path"].as<std::string>("");
                    reference.Enabled = referenceNode["Enabled"].as<bool>(true);
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
