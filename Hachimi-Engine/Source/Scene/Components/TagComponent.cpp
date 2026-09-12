#include "Scene/Components/TagComponent.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "TagComponent";

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<TagComponent>(entity);
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            (void)registry;
            (void)entity;
            HE_CORE_WARN("TagComponent cannot be removed; every entity needs a name");
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<TagComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<TagComponent>(sourceEntity))
            {
                target.emplace_or_replace<TagComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<TagComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << component->Tag;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            registry.get_or_emplace<TagComponent>(entity).Tag = node["Tag"].as<std::string>("Entity");
            return true;
        }
    }

    ComponentDescriptor MakeTagComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Tag";
        descriptor.TypeID = entt::type_hash<TagComponent>::value();
        descriptor.Required = true;
        // A duplicate is named by Scene::DuplicateEntity before the descriptors run, so cloning
        // the tag would overwrite the "Copy" suffix with the source name.
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
