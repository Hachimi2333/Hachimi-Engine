#include "Scene/Components/MeshRendererComponent.h"

#include "Core/Log.h"
#include "Scene/Components/AssetHandleSerialization.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

namespace HachimiEngine
{
    namespace
    {
        // The key is a format identifier, not a display label: it stayed "MeshComponent" when the
        // type was renamed to MeshRendererComponent, because renaming it would invalidate every
        // scene written before that rename for no benefit.
        constexpr const char* Key = "MeshComponent";
        // Retired key from format version 2. A file carrying it was hand-edited; the values that
        // matter are the ones below it.
        constexpr const char* LegacyMaterialOverrideKey = "MaterialOverride";

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

        // A mesh renderer without geometry draws nothing, and the primitive enum is the only thing
        // that says which geometry it should have, so the default brings the matching mesh with it.
        void AddDefault(entt::registry& registry, entt::entity entity)
        {
            // Guarded rather than overwriting: "add the default" must never discard the values a
            // component already has.
            if (registry.all_of<MeshRendererComponent>(entity))
            {
                return;
            }

            registry.emplace<MeshRendererComponent>(entity).SetPrimitive(PrimitiveMeshType::Cube);
        }

        void Remove(entt::registry& registry, entt::entity entity)
        {
            registry.remove<MeshRendererComponent>(entity);
        }

        bool Has(const entt::registry& registry, entt::entity entity)
        {
            return registry.all_of<MeshRendererComponent>(entity);
        }

        void Clone(const entt::registry& source, entt::entity sourceEntity,
                   entt::registry& target, entt::entity targetEntity)
        {
            const auto* component = source.try_get<MeshRendererComponent>(sourceEntity);
            if (component == nullptr)
            {
                return;
            }

            // Geometry and the material reference are both shared values: the mesh is immutable
            // CPU data and the handle names a project asset, so neither may be copied per entity.
            target.emplace_or_replace<MeshRendererComponent>(targetEntity, *component);
        }

        void Serialize(YAML::Emitter& out, const entt::registry& registry, entt::entity entity)
        {
            const auto* component = registry.try_get<MeshRendererComponent>(entity);
            if (component == nullptr)
            {
                return;
            }

            out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "PrimitiveType" << YAML::Value << EnumNames::ToName(component->Primitive, PrimitiveNames);
            AssetHandles::Emit(out, "Material", component->Material);
            out << YAML::Key << "AlbedoColor" << YAML::Value;
            EmitVec4(out, component->AlbedoColor);
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

            auto& component = registry.get_or_emplace<MeshRendererComponent>(entity);

            const std::string primitiveName = node["PrimitiveType"].as<std::string>("Cube");
            PrimitiveMeshType primitive = PrimitiveMeshType::Cube;
            if (!EnumNames::FromName(primitiveName, PrimitiveNames, primitive))
            {
                HE_CORE_WARN("Mesh primitive '{}' is not recognised; falling back to Cube", primitiveName);
                primitive = PrimitiveMeshType::Cube;
            }

            // The geometry follows the enum, so the mesh, its draw mode and its bounds cannot
            // disagree with what the file says the entity is.
            component.SetPrimitive(primitive);
            component.Material = AssetHandles::Read(node["Material"], AssetType::Material);

            if (node[LegacyMaterialOverrideKey])
            {
                // Format version 2 stored a runtime-only Material instance here under a key that
                // was never written. A file carrying it was hand-edited; the fields below are the
                // values that actually matter.
                HE_CORE_WARN("Mesh component holds the retired '{}' key; it is ignored",
                    LegacyMaterialOverrideKey);
            }

            component.AlbedoColor = ReadVec4(node["AlbedoColor"], component.AlbedoColor);
            component.Roughness = node["Roughness"].as<float>(0.6f);
            component.Metallic = node["Metallic"].as<float>(0.05f);
            component.Visible = node["Visible"].as<bool>(true);
            return true;
        }
    }

    ComponentDescriptor MakeMeshRendererComponentDescriptor()
    {
        ComponentDescriptor descriptor;
        descriptor.Name = Key;
        descriptor.DisplayName = "Mesh Renderer";
        descriptor.TypeID = entt::type_hash<MeshRendererComponent>::value();
        descriptor.AddDefault = &AddDefault;
        descriptor.Remove = &Remove;
        descriptor.Has = &Has;
        descriptor.Clone = &Clone;
        descriptor.Serialize = &Serialize;
        descriptor.Deserialize = &Deserialize;
        return descriptor;
    }

    MeshRendererComponent& MeshRendererComponent::SetPrimitive(PrimitiveMeshType primitive)
    {
        Primitive = primitive;
        Mesh = MeshFactory::CreatePrimitive(primitive);
        return *this;
    }
}
