#include "Scene/Components/ColliderComponent.h"

#include "Core/Log.h"
#include "Scene/Components/MeshComponent.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "ColliderComponent";

        constexpr std::array<EnumEntry<ColliderComponent::ColliderShapeType>, 4> ShapeNames {{
            { ColliderComponent::ColliderShapeType::Box, "Box" },
            { ColliderComponent::ColliderShapeType::Sphere, "Sphere" },
            { ColliderComponent::ColliderShapeType::Capsule, "Capsule" },
            { ColliderComponent::ColliderShapeType::Plane, "Plane" }
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

        void SizeForMeshPrimitive(entt::registry& registry, entt::entity entity, ColliderComponent& collider)
        {
            const auto* mesh = registry.try_get<MeshComponent>(entity);
            if (mesh == nullptr)
            {
                return;
            }

            switch (mesh->PrimitiveType)
            {
                case PrimitiveMeshType::Sphere:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
                    collider.Radius = 0.5f;
                    break;
                case PrimitiveMeshType::Plane:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Plane;
                    collider.HalfExtents = { 5.0f, 0.05f, 5.0f };
                    break;
                case PrimitiveMeshType::Cube:
                case PrimitiveMeshType::Grid:
                case PrimitiveMeshType::None:
                default:
                    collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
                    collider.HalfExtents = { 0.5f, 0.5f, 0.5f };
                    break;
            }
        }

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            auto& collider = registry.emplace<ColliderComponent>(entity);
            SizeForMeshPrimitive(registry, entity, collider);
        }

        void AddBoxPreset(entt::registry& registry, entt::entity entity)
        {
            auto& collider = registry.emplace<ColliderComponent>(entity);
            collider.ShapeType = ColliderComponent::ColliderShapeType::Box;
        }

        void AddSpherePreset(entt::registry& registry, entt::entity entity)
        {
            auto& collider = registry.emplace<ColliderComponent>(entity);
            collider.ShapeType = ColliderComponent::ColliderShapeType::Sphere;
        }

        void AddCapsulePreset(entt::registry& registry, entt::entity entity)
        {
            auto& collider = registry.emplace<ColliderComponent>(entity);
            collider.ShapeType = ColliderComponent::ColliderShapeType::Capsule;
            collider.Radius = 0.25f;
            collider.Height = 1.0f;
        }

        void AddPlanePreset(entt::registry& registry, entt::entity entity)
        {
            auto& collider = registry.emplace<ColliderComponent>(entity);
            collider.ShapeType = ColliderComponent::ColliderShapeType::Plane;
            collider.HalfExtents = { 5.0f, 0.05f, 5.0f };
        }

        constexpr std::array<ComponentAddPreset, 4> Presets {{
            { "Box Collider", &AddBoxPreset },
            { "Sphere Collider", &AddSpherePreset },
            { "Capsule Collider", &AddCapsulePreset },
            { "Plane Collider", &AddPlanePreset }
        }};

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<ColliderComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<ColliderComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            if (const auto* component = source.try_get<ColliderComponent>(sourceEntity))
            {
                target.emplace_or_replace<ColliderComponent>(targetEntity, *component);
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<ColliderComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ShapeType" << YAML::Value << EnumNames::ToName(component->ShapeType, ShapeNames);
            out << YAML::Key << "HalfExtents" << YAML::Value;
            EmitVec3(out, component->HalfExtents);
            out << YAML::Key << "Radius" << YAML::Value << component->Radius;
            out << YAML::Key << "Height" << YAML::Value << component->Height;
            out << YAML::Key << "Offset" << YAML::Value;
            EmitVec3(out, component->Offset);
            out << YAML::Key << "Density" << YAML::Value << component->Density;
            out << YAML::Key << "Friction" << YAML::Value << component->Friction;
            out << YAML::Key << "Restitution" << YAML::Value << component->Restitution;
            out << YAML::Key << "RollingResistance" << YAML::Value << component->RollingResistance;
            out << YAML::Key << "IsTrigger" << YAML::Value << component->IsTrigger;
            out << YAML::Key << "CategoryBits" << YAML::Value << component->CategoryBits;
            out << YAML::Key << "MaskBits" << YAML::Value << component->MaskBits;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<ColliderComponent>(entity);

            const std::string shapeName = node["ShapeType"].as<std::string>("Box");
            if (!EnumNames::FromName(shapeName, ShapeNames, component.ShapeType))
            {
                HE_CORE_WARN("Collider shape '{}' is not recognised; falling back to Box", shapeName);
                component.ShapeType = ColliderComponent::ColliderShapeType::Box;
            }

            component.HalfExtents = ReadVec3(node["HalfExtents"], component.HalfExtents);
            component.Radius = node["Radius"].as<float>(0.5f);
            component.Height = node["Height"].as<float>(1.0f);
            component.Offset = ReadVec3(node["Offset"], Math::Vec3(0.0f));
            component.Density = node["Density"].as<float>(1.0f);
            component.Friction = node["Friction"].as<float>(0.6f);
            component.Restitution = node["Restitution"].as<float>(0.0f);
            component.RollingResistance = node["RollingResistance"].as<float>(0.0f);
            component.IsTrigger = node["IsTrigger"].as<bool>(false);
            component.CategoryBits = node["CategoryBits"].as<uint64_t>(~0ull);
            component.MaskBits = node["MaskBits"].as<uint64_t>(~0ull);
            return true;
        }
    }

    void AddDefaultCollider(entt::registry& registry, entt::entity entity)
    {
        AddDefault(registry, entity);
    }

    ComponentDescriptor MakeColliderComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Collider";
        descriptor.TypeID = entt::type_hash<ColliderComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        descriptor.Presets = Presets;
        return descriptor;
    }
}
