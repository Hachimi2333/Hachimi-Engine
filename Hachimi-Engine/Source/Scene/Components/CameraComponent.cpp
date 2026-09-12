#include "Scene/Components/CameraComponent.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "CameraComponent";

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<CameraComponent>(entity);
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<CameraComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<CameraComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<CameraComponent>(sourceEntity))
            {
                target.emplace_or_replace<CameraComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<CameraComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Primary" << YAML::Value << component->Primary;
            out << YAML::Key << "FieldOfView" << YAML::Value << component->FieldOfView;
            out << YAML::Key << "NearClip" << YAML::Value << component->NearClip;
            out << YAML::Key << "FarClip" << YAML::Value << component->FarClip;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<CameraComponent>(entity);
            component.Primary = node["Primary"].as<bool>(false);
            component.FieldOfView = node["FieldOfView"].as<float>(45.0f);
            component.NearClip = node["NearClip"].as<float>(0.1f);
            component.FarClip = node["FarClip"].as<float>(1000.0f);
            return true;
        }
    }

    ComponentDescriptor MakeCameraComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Camera";
        descriptor.TypeID = entt::type_hash<CameraComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
