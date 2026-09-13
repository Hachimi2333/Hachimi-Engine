#include "Scene/Components/LightComponent.h"

#include "Core/Log.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "LightComponent";

        constexpr std::array<EnumEntry<LightComponent::LightType>, 2> TypeNames {{
            { LightComponent::LightType::Directional, "Directional" },
            { LightComponent::LightType::Point, "Point" }
        }};

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
            // Guarded rather than overwriting: "add the default" must never discard the values a
            // component already has.
            if (!registry.all_of<LightComponent>(entity))
            {
                registry.emplace<LightComponent>(entity);
            }
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<LightComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<LightComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<LightComponent>(sourceEntity))
            {
                target.emplace_or_replace<LightComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<LightComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << EnumNames::ToName(component->Type, TypeNames);
            out << YAML::Key << "Color" << YAML::Value;
            EmitVec3(out, component->Color);
            out << YAML::Key << "Intensity" << YAML::Value << component->Intensity;
            out << YAML::Key << "Range" << YAML::Value << component->Range;
            out << YAML::Key << "CastsShadows" << YAML::Value << component->CastsShadows;
            out << YAML::Key << "ShadowBias" << YAML::Value << component->ShadowBias;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<LightComponent>(entity);

            const std::string typeName = node["Type"].as<std::string>("Point");
            if (!EnumNames::FromName(typeName, TypeNames, component.Type))
            {
                HE_CORE_WARN("Light type '{}' is not recognised; falling back to Point", typeName);
                component.Type = LightComponent::LightType::Point;
            }

            component.Color = ReadVec3(node["Color"], Math::Vec3(1.0f));
            component.Intensity = node["Intensity"].as<float>(10.0f);
            component.Range = node["Range"].as<float>(12.0f);
            component.CastsShadows = node["CastsShadows"].as<bool>(true);
            component.ShadowBias = node["ShadowBias"].as<float>(0.0005f);
            return true;
        }
    }

    ComponentDescriptor MakeLightComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Light";
        descriptor.TypeID = entt::type_hash<LightComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
