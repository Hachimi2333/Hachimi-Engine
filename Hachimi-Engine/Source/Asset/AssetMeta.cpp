#include "Asset/AssetMeta.h"

#include "Core/Log.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* Key = "Meta";

        constexpr std::array<EnumEntry<AssetType>, 6> AssetTypeNames {{
            { AssetType::None, "None" },
            { AssetType::Texture, "Texture" },
            { AssetType::Material, "Material" },
            { AssetType::Scene, "Scene" },
            { AssetType::Script, "Script" },
            { AssetType::Project, "Project" }
        }};

        constexpr std::array<EnumEntry<TextureType>, 3> TextureTypeNames {{
            { TextureType::Color, "Color" },
            { TextureType::Normal, "Normal" },
            { TextureType::Data, "Data" }
        }};

        constexpr std::array<EnumEntry<TextureWrapMode>, 3> WrapModeNames {{
            { TextureWrapMode::Repeat, "Repeat" },
            { TextureWrapMode::Clamp, "Clamp" },
            { TextureWrapMode::MirroredRepeat, "MirroredRepeat" }
        }};

        constexpr std::array<EnumEntry<TextureFilterMode>, 2> FilterModeNames {{
            { TextureFilterMode::Nearest, "Nearest" },
            { TextureFilterMode::Linear, "Linear" }
        }};

        std::string ToLowerExtension(std::string_view extension)
        {
            std::string lowered(extension);
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return lowered;
        }
    }

    AssetMeta AssetMeta::MakeDefault(AssetType type)
    {
        AssetMeta meta;
        meta.ID = UUID();
        meta.Type = type;
        meta.Importer = type == AssetType::Texture ? "Texture" : "Native";
        return meta;
    }

    std::string AssetMeta::Serialize(const AssetMeta& meta)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << Key << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "ID" << YAML::Value << meta.ID.ToString();
        out << YAML::Key << "Type" << YAML::Value << EnumNames::ToName(meta.Type, AssetTypeNames);
        out << YAML::Key << "Importer" << YAML::Value << meta.Importer;

        if (meta.Type == AssetType::Texture)
        {
            // Only textures carry import settings today; a future importer adds its own block
            // here rather than inventing a second sidecar format.
            out << YAML::Key << "Texture" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << EnumNames::ToName(meta.Texture.Type, TextureTypeNames);
            out << YAML::Key << "GenerateMipmaps" << YAML::Value << meta.Texture.GenerateMipmaps;
            out << YAML::Key << "Wrap" << YAML::Value << EnumNames::ToName(meta.Texture.Wrap, WrapModeNames);
            out << YAML::Key << "Filter" << YAML::Value << EnumNames::ToName(meta.Texture.Filter, FilterModeNames);
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
        out << YAML::EndMap;
        return out.c_str();
    }

    bool AssetMeta::Deserialize(std::string_view text, AssetMeta& out)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(std::string(text));
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Asset metadata is not valid YAML: {}", exception.what());
            return false;
        }

        // A record has to be a map at the root, and a wrapped record has to be a map too. Indexing a
        // sequence with a string creates a node rather than failing, so the root type is checked
        // before any key is looked up.
        if (data.Type() != YAML::NodeType::Map)
        {
            return false;
        }

        const YAML::Node named = data[Key];
        const YAML::Node node = named.IsMap() ? named : data;
        if (!node.IsMap())
        {
            return false;
        }

        const std::string idText = node["ID"].as<std::string>("");
        if (idText.empty())
        {
            return false;
        }

        UUID parsedId = UUID::Invalid();
        try
        {
            parsedId = UUID(std::stoull(idText, nullptr, 16));
        }
        catch (const std::exception&)
        {
            HE_CORE_ERROR("Asset metadata has an unreadable ID: '{}'", idText);
            return false;
        }

        if (parsedId == UUID::Invalid())
        {
            return false;
        }

        AssetMeta meta;
        meta.ID = parsedId;

        const std::string typeName = node["Type"].as<std::string>("None");
        if (!EnumNames::FromName(typeName, AssetTypeNames, meta.Type) || meta.Type == AssetType::None)
        {
            // A record without a usable kind cannot be indexed, so it is regenerated.
            HE_CORE_ERROR("Asset metadata has an unknown type: '{}'", typeName);
            return false;
        }

        meta.Importer = node["Importer"].as<std::string>("Native");

        if (const YAML::Node textureNode = node["Texture"])
        {
            const std::string textureTypeName = textureNode["Type"].as<std::string>("Color");
            if (!EnumNames::FromName(textureTypeName, TextureTypeNames, meta.Texture.Type))
            {
                HE_CORE_WARN("Unknown texture type '{}' in asset metadata; using Color", textureTypeName);
            }

            meta.Texture.GenerateMipmaps = textureNode["GenerateMipmaps"].as<bool>(true);

            const std::string wrapName = textureNode["Wrap"].as<std::string>("Repeat");
            if (!EnumNames::FromName(wrapName, WrapModeNames, meta.Texture.Wrap))
            {
                HE_CORE_WARN("Unknown texture wrap mode '{}' in asset metadata; using Repeat", wrapName);
            }

            const std::string filterName = textureNode["Filter"].as<std::string>("Linear");
            if (!EnumNames::FromName(filterName, FilterModeNames, meta.Texture.Filter))
            {
                HE_CORE_WARN("Unknown texture filter mode '{}' in asset metadata; using Linear", filterName);
            }
        }

        out = meta;
        return true;
    }

    std::filesystem::path GetAssetMetaPath(const std::filesystem::path& assetPath)
    {
        std::filesystem::path metaPath = assetPath;
        metaPath += ".meta";
        return metaPath;
    }

    bool IsAssetMetaPath(const std::filesystem::path& path)
    {
        return path.extension() == ".meta";
    }

    AssetType GetAssetTypeForExtension(std::string_view extension)
    {
        const std::string lowered = ToLowerExtension(extension);

        if (lowered == ".png" || lowered == ".jpg" || lowered == ".jpeg"
            || lowered == ".tga" || lowered == ".bmp")
        {
            return AssetType::Texture;
        }
        if (lowered == ".hmaterial")
        {
            return AssetType::Material;
        }
        if (lowered == ".hscene")
        {
            return AssetType::Scene;
        }
        if (lowered == ".lua")
        {
            return AssetType::Script;
        }
        if (lowered == ".hproj")
        {
            return AssetType::Project;
        }

        return AssetType::None;
    }
}
