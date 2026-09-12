#include "Scene/Components/RigidbodyComponent.h"

#include "Core/Log.h"
#include "Scene/Components/ColliderComponent.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "RigidbodyComponent";

        constexpr std::array<EnumEntry<RigidbodyComponent::RigidbodyType>, 3> TypeNames {{
            { RigidbodyComponent::RigidbodyType::Static, "Static" },
            { RigidbodyComponent::RigidbodyType::Kinematic, "Kinematic" },
            { RigidbodyComponent::RigidbodyType::Dynamic, "Dynamic" }
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

        // A rigid body without a collider cannot be simulated, and the two are authored
        // together in the editor, so adding one adds the other.
        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            registry.emplace<RigidbodyComponent>(entity);

            if (!registry.all_of<ColliderComponent>(entity))
            {
                AddDefaultCollider(registry, entity);
            }
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<RigidbodyComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<RigidbodyComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<RigidbodyComponent>(sourceEntity))
            {
                target.emplace_or_replace<RigidbodyComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<RigidbodyComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << EnumNames::ToName(component->Type, TypeNames);
            out << YAML::Key << "LinearVelocity" << YAML::Value;
            EmitVec3(out, component->LinearVelocity);
            out << YAML::Key << "AngularVelocity" << YAML::Value;
            EmitVec3(out, component->AngularVelocity);
            out << YAML::Key << "LinearDamping" << YAML::Value << component->LinearDamping;
            out << YAML::Key << "AngularDamping" << YAML::Value << component->AngularDamping;
            out << YAML::Key << "GravityScale" << YAML::Value << component->GravityScale;
            out << YAML::Key << "EnableSleep" << YAML::Value << component->EnableSleep;
            out << YAML::Key << "InitiallyAwake" << YAML::Value << component->InitiallyAwake;
            out << YAML::Key << "IsBullet" << YAML::Value << component->IsBullet;
            out << YAML::Key << "IsEnabled" << YAML::Value << component->IsEnabled;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<RigidbodyComponent>(entity);

            const std::string typeName = node["Type"].as<std::string>("Dynamic");
            if (!EnumNames::FromName(typeName, TypeNames, component.Type))
            {
                HE_CORE_WARN("Rigidbody type '{}' is not recognised; falling back to Dynamic", typeName);
                component.Type = RigidbodyComponent::RigidbodyType::Dynamic;
            }

            component.LinearVelocity = ReadVec3(node["LinearVelocity"], Math::Vec3(0.0f));
            component.AngularVelocity = ReadVec3(node["AngularVelocity"], Math::Vec3(0.0f));
            component.LinearDamping = node["LinearDamping"].as<float>(0.0f);
            component.AngularDamping = node["AngularDamping"].as<float>(0.0f);
            component.GravityScale = node["GravityScale"].as<float>(1.0f);
            component.EnableSleep = node["EnableSleep"].as<bool>(true);
            component.InitiallyAwake = node["InitiallyAwake"].as<bool>(true);
            component.IsBullet = node["IsBullet"].as<bool>(false);
            component.IsEnabled = node["IsEnabled"].as<bool>(true);
            return true;
        }
    }

    ComponentDescriptor MakeRigidbodyComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Rigidbody";
        descriptor.TypeID = entt::type_hash<RigidbodyComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
