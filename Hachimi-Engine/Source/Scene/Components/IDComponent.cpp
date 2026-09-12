#include "Scene/Components/IDComponent.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        // The UUID is the entity map's own key rather than a nested block, so this component
        // is the one that writes it.
        constexpr const char* EntityKey = "Entity";

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
                HE_CORE_ERROR("Scene contains an unreadable entity UUID: '{}'", node.as<std::string>());
                return UUID::Invalid();
            }
        }

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<IDComponent>(entity).ID = UUID();
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            (void)registry;
            (void)entity;
            HE_CORE_WARN("IDComponent cannot be removed; it is what identifies the entity");
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<IDComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<IDComponent>(sourceEntity))
            {
                target.emplace_or_replace<IDComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<IDComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << EntityKey << YAML::Value << component->ID.ToString();
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const UUID id = ParseUUID(entityNode[EntityKey]);
            if (id == UUID::Invalid())
            {
                return false;
            }

            registry.get_or_emplace<IDComponent>(entity).ID = id;
            return true;
        }
    }

    ComponentDescriptor MakeIDComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = "IDComponent";
        descriptor.DisplayName = "ID";
        descriptor.TypeID = entt::type_hash<IDComponent>::value();
        descriptor.Required = true;
        // A duplicate gets its own identity.
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
