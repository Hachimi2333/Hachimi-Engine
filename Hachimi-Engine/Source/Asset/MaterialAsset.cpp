#include "Asset/MaterialAsset.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "Material";

        float ClampUnit(float value)
        {
            if (!std::isfinite(value))
            {
                return 0.0f;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

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

            return {
                ClampUnit(node[0].as<float>(fallback.x)),
                ClampUnit(node[1].as<float>(fallback.y)),
                ClampUnit(node[2].as<float>(fallback.z)),
                ClampUnit(node[3].as<float>(fallback.w))
            };
        }

        // Texture reference is stored as the asset UUID, and read back only when it names one.
        AssetHandle ReadTextureHandle(const YAML::Node& node)
        {
            const std::string idText = node.as<std::string>("");
            if (idText.empty())
            {
                return AssetHandle::Invalid();
            }

            try
            {
                const UUID id(std::stoull(idText, nullptr, 16));
                if (id == UUID::Invalid())
                {
                    return AssetHandle::Invalid();
                }
                return AssetHandle::From(id, AssetType::Texture);
            }
            catch (const std::exception&)
            {
                HE_CORE_WARN("Material albedo texture reference is not a UUID: '{}'", idText);
                return AssetHandle::Invalid();
            }
        }
    }

    std::string MaterialAsset::Serialize(const MaterialAsset& material)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Shader" << YAML::Value << material.Shader;
        out << YAML::Key << "BaseColor" << YAML::Value;
        EmitVec4(out, material.BaseColor);

        if (material.AlbedoTexture.IsValid())
        {
            out << YAML::Key << "AlbedoTexture" << YAML::Value << material.AlbedoTexture.ID.ToString();
        }

        out << YAML::Key << "Roughness" << YAML::Value << material.Roughness;
        out << YAML::Key << "Metallic" << YAML::Value << material.Metallic;
        out << YAML::EndMap;
        out << YAML::EndMap;
        return out.c_str();
    }

    bool MaterialAsset::Deserialize(std::string_view text, MaterialAsset& out)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(std::string(text));
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Material asset is not valid YAML: {}", exception.what());
            return false;
        }

        // The document has to be a map at the root, and a wrapped document has to be a map too.
        // Indexing a sequence with a string creates a node, so the root type is checked before any
        // key is looked up rather than trusting what the lookup returns.
        if (data.Type() != YAML::NodeType::Map)
        {
            HE_CORE_ERROR("A material document must be a map, but it is {}",
                data.Type() == YAML::NodeType::Sequence ? "a sequence" : "not a mapping");
            return false;
        }

        const YAML::Node named = data[Key];
        const YAML::Node node = named.IsMap() ? named : data;
        if (!node.IsMap())
        {
            HE_CORE_ERROR("A material document must be a map");
            return false;
        }

        MaterialAsset material;
        material.Shader = node["Shader"].as<std::string>(DefaultShaderName);
        if (material.Shader.empty())
        {
            material.Shader = DefaultShaderName;
        }

        material.BaseColor = ReadVec4(node["BaseColor"], material.BaseColor);
        material.AlbedoTexture = ReadTextureHandle(node["AlbedoTexture"]);
        material.Roughness = ClampUnit(node["Roughness"].as<float>(material.Roughness));
        material.Metallic = ClampUnit(node["Metallic"].as<float>(material.Metallic));

        out = material;
        return true;
    }
}
