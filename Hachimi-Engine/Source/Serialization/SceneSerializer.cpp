#include "Serialization/SceneSerializer.h"

#include "Core/Log.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <glm/glm.hpp>

#include <fstream>

namespace HachimiEngine
{
    namespace
    {
        void EmitVec3(YAML::Emitter& out, const glm::vec3& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
        }

        void EmitVec4(YAML::Emitter& out, const glm::vec4& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
        }

        glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback = glm::vec3(0.0f))
        {
            if (!node || !node.IsSequence() || node.size() < 3)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
        }

        glm::vec4 ReadVec4(const YAML::Node& node, const glm::vec4& fallback = glm::vec4(1.0f))
        {
            if (!node || !node.IsSequence() || node.size() < 4)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() };
        }
    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
    }

    void SceneSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        for (const Entity entity : m_Scene->GetAllEntities())
        {
            SerializeEntity(out, entity);
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        file << out.c_str();
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        const YAML::Node data = YAML::LoadFile(filepath);
        if (!data || !data["Scene"])
        {
            HE_CORE_ERROR("Failed to load scene file: {}", filepath);
            return false;
        }

        m_Scene->m_EntityMap.clear();
        m_Scene->m_Registry.clear();
        m_Scene->SetName(data["Scene"].as<std::string>());

        const YAML::Node entities = data["Entities"];
        if (entities && entities.IsSequence())
        {
            for (const YAML::Node entityNode : entities)
            {
                DeserializeEntity(entityNode, *m_Scene);
            }
        }

        return true;
    }

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID().ToString();

        if (entity.HasComponent<TagComponent>())
        {
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TransformComponent>())
        {
            const auto& transform = entity.GetComponent<TransformComponent>();
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value;
            EmitVec3(out, transform.Position);
            out << YAML::Key << "Rotation" << YAML::Value;
            EmitVec3(out, transform.Rotation);
            out << YAML::Key << "Scale" << YAML::Value;
            EmitVec3(out, transform.Scale);
            out << YAML::EndMap;
        }

        if (entity.HasComponent<RelationshipComponent>())
        {
            const auto& relationship = entity.GetComponent<RelationshipComponent>();
            out << YAML::Key << "RelationshipComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value << relationship.Parent.ToString();

            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            for (const UUID child : relationship.Children)
            {
                out << child.ToString();
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MeshComponent>())
        {
            const auto& mesh = entity.GetComponent<MeshComponent>();
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "PrimitiveType" << YAML::Value << static_cast<int>(mesh.PrimitiveType);
            out << YAML::Key << "AlbedoColor" << YAML::Value;
            EmitVec4(out, mesh.MaterialColor);
            out << YAML::Key << "Roughness" << YAML::Value << mesh.Roughness;
            out << YAML::Key << "Metallic" << YAML::Value << mesh.Metallic;
            out << YAML::Key << "Visible" << YAML::Value << mesh.Visible;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& camera = entity.GetComponent<CameraComponent>();
            out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Primary" << YAML::Value << camera.Primary;
            out << YAML::Key << "FieldOfView" << YAML::Value << camera.FieldOfView;
            out << YAML::Key << "NearClip" << YAML::Value << camera.NearClip;
            out << YAML::Key << "FarClip" << YAML::Value << camera.FarClip;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<LightComponent>())
        {
            const auto& light = entity.GetComponent<LightComponent>();
            out << YAML::Key << "LightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << static_cast<int>(light.Type);
            out << YAML::Key << "Color" << YAML::Value;
            EmitVec3(out, light.Color);
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    void SceneSerializer::DeserializeEntity(YAML::Node entityNode, Scene& scene)
    {
        const uint64_t uuidValue = std::stoull(entityNode["Entity"].as<std::string>(), nullptr, 16);
        Entity entity(scene.m_Registry.create(), &scene);
        entity.AddComponent<IDComponent>().ID = UUID(uuidValue);

        if (const YAML::Node tagNode = entityNode["TagComponent"])
        {
            entity.AddComponent<TagComponent>().Tag = tagNode["Tag"].as<std::string>("Entity");
        }

        entity.AddComponent<TransformComponent>();
        if (const YAML::Node transformNode = entityNode["TransformComponent"])
        {
            auto& transform = entity.GetComponent<TransformComponent>();
            transform.Position = ReadVec3(transformNode["Position"]);
            transform.Rotation = ReadVec3(transformNode["Rotation"]);
            transform.Scale = ReadVec3(transformNode["Scale"], glm::vec3(1.0f));
        }

        entity.AddComponent<RelationshipComponent>();
        if (const YAML::Node relationshipNode = entityNode["RelationshipComponent"])
        {
            auto& relationship = entity.GetComponent<RelationshipComponent>();
            const uint64_t parentUUID = std::stoull(relationshipNode["Parent"].as<std::string>("0"), nullptr, 16);
            relationship.Parent = UUID(parentUUID);

            if (const YAML::Node childrenNode = relationshipNode["Children"])
            {
                for (const YAML::Node childNode : childrenNode)
                {
                    const uint64_t childUUID = std::stoull(childNode.as<std::string>(), nullptr, 16);
                    relationship.Children.push_back(UUID(childUUID));
                }
            }
        }

        if (const YAML::Node meshNode = entityNode["MeshComponent"])
        {
            auto& mesh = entity.AddComponent<MeshComponent>();
            mesh.PrimitiveType = static_cast<PrimitiveMeshType>(meshNode["PrimitiveType"].as<int>(1));
            mesh.Mesh = MeshFactory::CreatePrimitive(mesh.PrimitiveType);
            mesh.MaterialColor = ReadVec4(meshNode["AlbedoColor"], mesh.MaterialColor);
            mesh.Roughness = meshNode["Roughness"].as<float>(0.6f);
            mesh.Metallic = meshNode["Metallic"].as<float>(0.05f);
            mesh.Visible = meshNode["Visible"].as<bool>(true);

            if (SceneRenderer::GetDefaultMaterial() != nullptr)
            {
                mesh.MaterialOverride = Material::Create(SceneRenderer::GetDefaultMaterial()->GetShader());
                mesh.MaterialOverride->SetAlbedoColor(mesh.MaterialColor);
                mesh.MaterialOverride->SetRoughness(mesh.Roughness);
                mesh.MaterialOverride->SetMetallic(mesh.Metallic);
            }
        }

        if (const YAML::Node cameraNode = entityNode["CameraComponent"])
        {
            auto& camera = entity.AddComponent<CameraComponent>();
            camera.Primary = cameraNode["Primary"].as<bool>(false);
            camera.FieldOfView = cameraNode["FieldOfView"].as<float>(45.0f);
            camera.NearClip = cameraNode["NearClip"].as<float>(0.1f);
            camera.FarClip = cameraNode["FarClip"].as<float>(1000.0f);
        }

        if (const YAML::Node lightNode = entityNode["LightComponent"])
        {
            auto& light = entity.AddComponent<LightComponent>();
            light.Type = static_cast<LightComponent::LightType>(lightNode["Type"].as<int>(1));
            light.Color = ReadVec3(lightNode["Color"], glm::vec3(1.0f));
            light.Intensity = lightNode["Intensity"].as<float>(10.0f);
        }

        scene.m_EntityMap[UUID(uuidValue)] = entity.GetHandle();
    }
}
