#include "Scene/Components/TransformComponent.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "TransformComponent";

        void EmitVec3(YAML::Emitter& out, const Math::Vec3& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
        }

        Math::Vec3 ReadVec3(const YAML::Node& node, const Math::Vec3& fallback)
        {
            if (!node || !node.IsSequence() || node.size() < 3)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
        }

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<TransformComponent>(entity);
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            (void)registry;
            (void)entity;
            HE_CORE_WARN("TransformComponent cannot be removed; rendering and physics both need it");
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<TransformComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<TransformComponent>(sourceEntity))
            {
                target.emplace_or_replace<TransformComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<TransformComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value;
            EmitVec3(out, component->Position);
            out << YAML::Key << "Rotation" << YAML::Value;
            EmitVec3(out, component->Rotation);
            out << YAML::Key << "Scale" << YAML::Value;
            EmitVec3(out, component->Scale);
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<TransformComponent>(entity);
            component.Position = ReadVec3(node["Position"], Math::Vec3(0.0f));
            component.Rotation = ReadVec3(node["Rotation"], Math::Vec3(0.0f));
            component.Scale = ReadVec3(node["Scale"], Math::Vec3(1.0f));
            return true;
        }
    }

    ComponentDescriptor MakeTransformComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Transform";
        descriptor.TypeID = entt::type_hash<TransformComponent>::value();
        descriptor.Required = true;
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
