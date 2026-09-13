#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Log.h"
#include "Serialization/EnumNames.h"

#include <yaml-cpp/yaml.h>

#include <array>
#include <string>

namespace HachimiEngine
{
    // Reads and writes an asset reference inside a .hscene component block.
    //
    // A reference is stored as the asset UUID, and the kind is implied by the field it appears
    // in - a material field can only hold a material. That keeps the file readable, and a
    // reference to an asset this project does not have still has a value: it stays in the scene
    // as a missing reference instead of being silently dropped, which is what lets a scene be
    // opened before its assets are imported.
    namespace AssetHandles
    {
        inline constexpr std::array<EnumEntry<AssetType>, 5> AssetTypeNames {{
            { AssetType::Texture, "Texture" },
            { AssetType::Material, "Material" },
            { AssetType::Scene, "Scene" },
            { AssetType::Script, "Script" },
            { AssetType::Project, "Project" }
        }};

        inline void Emit(YAML::Emitter& out, const char* key, AssetHandle handle)
        {
            if (handle.IsValid())
            {
                out << YAML::Key << key << YAML::Value << handle.ID.ToString();
            }
        }

        inline AssetHandle Read(const YAML::Node& node, AssetType type)
        {
            const std::string text = node.as<std::string>("");
            if (text.empty())
            {
                return AssetHandle::Invalid();
            }

            try
            {
                const UUID id(std::stoull(text, nullptr, 16));
                if (id == UUID::Invalid())
                {
                    return AssetHandle::Invalid();
                }
                return AssetHandle::From(id, type);
            }
            catch (const std::exception&)
            {
                HE_CORE_WARN("Asset reference '{}' is not a UUID", text);
                return AssetHandle::Invalid();
            }
        }
    }
}
