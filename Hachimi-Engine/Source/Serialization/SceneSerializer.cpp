#include "Serialization/SceneSerializer.h"

#include "Core/Log.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <fstream>

namespace HachimiEngine
{
    namespace
    {
        void EmitVec3(YAML::Emitter& out, const Math::Vec3& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
        }

        void EmitVec4(YAML::Emitter& out, const Math::Vec4& value)
        {
            out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
        }

        Math::Vec3 ReadVec3(const YAML::Node& node, const Math::Vec3& fallback = Math::Vec3(0.0f))
        {
            if (!node || !node.IsSequence() || node.size() < 3)
            {
                return fallback;
            }
            return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
        }

        Math::Vec4 ReadVec4(const YAML::Node& node, const Math::Vec4& fallback = Math::Vec4(1.0f))
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

        const EnvironmentSettings& environment = m_Scene->GetEnvironmentSettings();
        out << YAML::Key << "Environment" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "ShowSkybox" << YAML::Value << environment.ShowSkybox;
        out << YAML::Key << "Exposure" << YAML::Value << environment.Exposure;
        out << YAML::Key << "EnvironmentIntensity" << YAML::Value << environment.EnvironmentIntensity;
        out << YAML::EndMap;

        const PhysicsSettings& physics = m_Scene->GetPhysicsSettings();
        out << YAML::Key << "Physics" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Gravity" << YAML::Value;
        EmitVec3(out, physics.Gravity);
        out << YAML::Key << "FixedTimeStep" << YAML::Value << physics.FixedTimeStep;
        out << YAML::Key << "SubStepCount" << YAML::Value << physics.SubStepCount;
        out << YAML::Key << "EnableSleep" << YAML::Value << physics.EnableSleep;
        out << YAML::Key << "EnableContinuous" << YAML::Value << physics.EnableContinuous;
        out << YAML::EndMap;

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

        m_Scene->OnRuntimeStop();
        m_Scene->m_EntityMap.clear();
        m_Scene->m_Registry.clear();
        m_Scene->SetName(data["Scene"].as<std::string>());

        if (const YAML::Node environmentNode = data["Environment"])
        {
            EnvironmentSettings& environment = m_Scene->GetEnvironmentSettings();
            environment.ShowSkybox = environmentNode["ShowSkybox"].as<bool>(true);
            environment.Exposure = environmentNode["Exposure"].as<float>(1.0f);
            environment.EnvironmentIntensity = environmentNode["EnvironmentIntensity"].as<float>(1.0f);
        }

        if (const YAML::Node physicsNode = data["Physics"])
        {
            PhysicsSettings& physics = m_Scene->GetPhysicsSettings();
            physics.Gravity = ReadVec3(physicsNode["Gravity"], physics.Gravity);
            physics.FixedTimeStep = physicsNode["FixedTimeStep"].as<float>(physics.FixedTimeStep);
            physics.SubStepCount = physicsNode["SubStepCount"].as<int>(physics.SubStepCount);
            physics.EnableSleep = physicsNode["EnableSleep"].as<bool>(physics.EnableSleep);
            physics.EnableContinuous = physicsNode["EnableContinuous"].as<bool>(physics.EnableContinuous);
        }

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

        if (entity.HasComponent<RigidbodyComponent>())
        {
            const auto& rigidbody = entity.GetComponent<RigidbodyComponent>();
            out << YAML::Key << "RigidbodyComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << static_cast<int>(rigidbody.Type);
            out << YAML::Key << "LinearVelocity" << YAML::Value;
            EmitVec3(out, rigidbody.LinearVelocity);
            out << YAML::Key << "AngularVelocity" << YAML::Value;
            EmitVec3(out, rigidbody.AngularVelocity);
            out << YAML::Key << "LinearDamping" << YAML::Value << rigidbody.LinearDamping;
            out << YAML::Key << "AngularDamping" << YAML::Value << rigidbody.AngularDamping;
            out << YAML::Key << "GravityScale" << YAML::Value << rigidbody.GravityScale;
            out << YAML::Key << "EnableSleep" << YAML::Value << rigidbody.EnableSleep;
            out << YAML::Key << "InitiallyAwake" << YAML::Value << rigidbody.InitiallyAwake;
            out << YAML::Key << "IsBullet" << YAML::Value << rigidbody.IsBullet;
            out << YAML::Key << "IsEnabled" << YAML::Value << rigidbody.IsEnabled;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<ColliderComponent>())
        {
            const auto& collider = entity.GetComponent<ColliderComponent>();
            out << YAML::Key << "ColliderComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ShapeType" << YAML::Value << static_cast<int>(collider.ShapeType);
            out << YAML::Key << "HalfExtents" << YAML::Value;
            EmitVec3(out, collider.HalfExtents);
            out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
            out << YAML::Key << "Height" << YAML::Value << collider.Height;
            out << YAML::Key << "Offset" << YAML::Value;
            EmitVec3(out, collider.Offset);
            out << YAML::Key << "Density" << YAML::Value << collider.Density;
            out << YAML::Key << "Friction" << YAML::Value << collider.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << collider.Restitution;
            out << YAML::Key << "RollingResistance" << YAML::Value << collider.RollingResistance;
            out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger;
            out << YAML::Key << "CategoryBits" << YAML::Value << collider.CategoryBits;
            out << YAML::Key << "MaskBits" << YAML::Value << collider.MaskBits;
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
            out << YAML::Key << "Range" << YAML::Value << light.Range;
            out << YAML::Key << "CastsShadows" << YAML::Value << light.CastsShadows;
            out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<ScriptComponent>())
        {
            const auto& script = entity.GetComponent<ScriptComponent>();
            out << YAML::Key << "ScriptComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;
            for (const ScriptComponent::ScriptReference& reference : script.Scripts)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Path" << YAML::Value << reference.Path;
                out << YAML::Key << "Enabled" << YAML::Value << reference.Enabled;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
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
            transform.Scale = ReadVec3(transformNode["Scale"], Math::Vec3(1.0f));
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

        if (const YAML::Node rigidbodyNode = entityNode["RigidbodyComponent"])
        {
            auto& rigidbody = entity.AddComponent<RigidbodyComponent>();
            rigidbody.Type = static_cast<RigidbodyComponent::RigidbodyType>(rigidbodyNode["Type"].as<int>(static_cast<int>(rigidbody.Type)));
            rigidbody.LinearVelocity = ReadVec3(rigidbodyNode["LinearVelocity"]);
            rigidbody.AngularVelocity = ReadVec3(rigidbodyNode["AngularVelocity"]);
            rigidbody.LinearDamping = rigidbodyNode["LinearDamping"].as<float>(0.0f);
            rigidbody.AngularDamping = rigidbodyNode["AngularDamping"].as<float>(0.0f);
            rigidbody.GravityScale = rigidbodyNode["GravityScale"].as<float>(1.0f);
            rigidbody.EnableSleep = rigidbodyNode["EnableSleep"].as<bool>(true);
            rigidbody.InitiallyAwake = rigidbodyNode["InitiallyAwake"].as<bool>(true);
            rigidbody.IsBullet = rigidbodyNode["IsBullet"].as<bool>(false);
            rigidbody.IsEnabled = rigidbodyNode["IsEnabled"].as<bool>(true);
        }

        if (const YAML::Node colliderNode = entityNode["ColliderComponent"])
        {
            auto& collider = entity.AddComponent<ColliderComponent>();
            collider.ShapeType = static_cast<ColliderComponent::ColliderShapeType>(colliderNode["ShapeType"].as<int>(static_cast<int>(collider.ShapeType)));
            collider.HalfExtents = ReadVec3(colliderNode["HalfExtents"], collider.HalfExtents);
            collider.Radius = colliderNode["Radius"].as<float>(0.5f);
            collider.Height = colliderNode["Height"].as<float>(1.0f);
            collider.Offset = ReadVec3(colliderNode["Offset"]);
            collider.Density = colliderNode["Density"].as<float>(1.0f);
            collider.Friction = colliderNode["Friction"].as<float>(0.6f);
            collider.Restitution = colliderNode["Restitution"].as<float>(0.0f);
            collider.RollingResistance = colliderNode["RollingResistance"].as<float>(0.0f);
            collider.IsTrigger = colliderNode["IsTrigger"].as<bool>(false);
            collider.CategoryBits = colliderNode["CategoryBits"].as<uint64_t>(~0ull);
            collider.MaskBits = colliderNode["MaskBits"].as<uint64_t>(~0ull);
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
            light.Color = ReadVec3(lightNode["Color"], Math::Vec3(1.0f));
            light.Intensity = lightNode["Intensity"].as<float>(10.0f);
            light.Range = lightNode["Range"].as<float>(12.0f);
            light.CastsShadows = lightNode["CastsShadows"].as<bool>(true);
            light.ShadowBias = lightNode["ShadowBias"].as<float>(0.0005f);
        }

        if (const YAML::Node scriptNode = entityNode["ScriptComponent"])
        {
            auto& script = entity.AddComponent<ScriptComponent>();
            if (const YAML::Node scriptsNode = scriptNode["Scripts"]; scriptsNode && scriptsNode.IsSequence())
            {
                for (const YAML::Node referenceNode : scriptsNode)
                {
                    ScriptComponent::ScriptReference reference;
                    reference.Path = referenceNode["Path"].as<std::string>("");
                    reference.Enabled = referenceNode["Enabled"].as<bool>(true);
                    script.Scripts.push_back(std::move(reference));
                }
            }
        }

        scene.m_EntityMap[UUID(uuidValue)] = entity.GetHandle();
    }
}
