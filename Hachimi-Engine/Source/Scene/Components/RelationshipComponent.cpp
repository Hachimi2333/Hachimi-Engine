#include "Scene/Components/RelationshipComponent.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "RelationshipComponent";

        UUID ParseUUID(const YAML::Node& node)
        {
            if (!node || !node.IsScalar())
            {
                return UUID::Invalid();
            }

            try
            {
                return UUID(std::stoull(node.as<std::string>(), nullptr, 16));
            }
            catch (const std::exception&)
            {
                HE_CORE_ERROR("Relationship component holds an unreadable parent UUID: '{}'", node.as<std::string>());
                return UUID::Invalid();
            }
        }

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<RelationshipComponent>(entity);
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            (void)registry;
            (void)entity;
            HE_CORE_WARN("RelationshipComponent cannot be removed; every entity has a place in the hierarchy");
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<RelationshipComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<RelationshipComponent>(sourceEntity))
            {
                target.emplace_or_replace<RelationshipComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<RelationshipComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value << component->Parent.ToString();
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<RelationshipComponent>(entity);
            component.Parent = ParseUUID(node["Parent"]);
            return true;
        }
    }

    ComponentDescriptor MakeRelationshipComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Relationship";
        descriptor.TypeID = entt::type_hash<RelationshipComponent>::value();
        descriptor.Required = true;
        // A duplicate is not a child of anything: it is placed next to its source.
        descriptor.CopiedOnDuplicate = false;
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
