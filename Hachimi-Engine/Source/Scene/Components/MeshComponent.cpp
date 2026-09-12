#include "Scene/Components/MeshComponent.h"

#include "Core/Log.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "MeshComponent";

        constexpr std::array<EnumEntry<PrimitiveMeshType>, 5> PrimitiveNames {{
            { PrimitiveMeshType::None, "None" },
            { PrimitiveMeshType::Cube, "Cube" },
            { PrimitiveMeshType::Sphere, "Sphere" },
            { PrimitiveMeshType::Plane, "Plane" },
            { PrimitiveMeshType::Grid, "Grid" }
        }};

        void EmitVec4(YAML::Emitter& out, const Math::Vec4& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
        }

        Math::Vec4 ReadVec4(const YAML::Node& node, const Math::Vec4& fallback)
        {
            if (!node || !node.IsSequence() || node.size() < 4)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() };
        }

        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            auto& component = registry.emplace<MeshComponent>(entity);
            component.PrimitiveType = PrimitiveMeshType::Cube;
            component.Mesh = MeshFactory::CreateCube();
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<MeshComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<MeshComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            const auto* component = source.try_get<MeshComponent>(sourceEntity);
            if (component == nullptr)
            {
                return;
            }

            auto& clone = target.emplace_or_replace<MeshComponent>(targetEntity, *component);

            // Geometry stays shared, but the material is cloned: runtime edits to one entity
            // must not change the other.
            if (component->MaterialOverride != nullptr)
            {
                const Ref<Material>& sourceMaterial = component->MaterialOverride;
                clone.MaterialOverride = Material::Create(sourceMaterial->GetShader());
                clone.MaterialOverride->SetAlbedoTexture(sourceMaterial->GetAlbedoTexture());
                clone.MaterialOverride->SetAlbedoColor(sourceMaterial->GetAlbedoColor());
                clone.MaterialOverride->SetRoughness(sourceMaterial->GetRoughness());
                clone.MaterialOverride->SetMetallic(sourceMaterial->GetMetallic());
            }
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<MeshComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "PrimitiveType" << YAML::Value << EnumNames::ToName(component->PrimitiveType, PrimitiveNames);
            out << YAML::Key << "AlbedoColor" << YAML::Value;
            EmitVec4(out, component->MaterialColor);
            out << YAML::Key << "Roughness" << YAML::Value << component->Roughness;
            out << YAML::Key << "Metallic" << YAML::Value << component->Metallic;
            out << YAML::Key << "Visible" << YAML::Value << component->Visible;
            out << YAML::EndMap;
        }

        bool Deserialize(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity)
        {
            const YAML::Node node = entityNode[Key];
            if (!node)
            {
                return false;
            }

            auto& component = registry.get_or_emplace<MeshComponent>(entity);

            const std::string primitiveName = node["PrimitiveType"].as<std::string>("Cube");
            if (!EnumNames::FromName(primitiveName, PrimitiveNames, component.PrimitiveType))
            {
                HE_CORE_WARN("Mesh primitive '{}' is not recognised; falling back to Cube", primitiveName);
                component.PrimitiveType = PrimitiveMeshType::Cube;
            }

            component.Mesh = MeshFactory::CreatePrimitive(component.PrimitiveType);
            component.MaterialColor = ReadVec4(node["AlbedoColor"], component.MaterialColor);
            component.Roughness = node["Roughness"].as<float>(0.6f);
            component.Metallic = node["Metallic"].as<float>(0.05f);
            component.Visible = node["Visible"].as<bool>(true);

            // No material instance is created here: the surface values above are what the
            // renderer draws with, so serialization stays free of any renderer dependency.
            return true;
        }
    }

    ComponentDescriptor MakeMeshComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Mesh";
        descriptor.TypeID = entt::type_hash<MeshComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }
}
