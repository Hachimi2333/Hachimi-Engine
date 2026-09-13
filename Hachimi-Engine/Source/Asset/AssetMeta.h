#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // Sampling state for a texture asset, authored in the Content Browser and applied when the
    // texture is uploaded. It deliberately holds no per-format option: textures are packaged as
    // the original file, so "import settings" describe how to interpret those bytes.
    enum class TextureWrapMode
    {
        Repeat = 0,
        Clamp = 1,
        MirroredRepeat = 2
    };

    enum class TextureFilterMode
    {
        Nearest = 0,
        Linear = 1
    };

    // Role of the pixels, which decides the colour space. Normal maps and data maps (AO,
    // roughness) must not be decoded as sRGB, or the values are wrong before the shader sees them.
    enum class TextureType
    {
        Color = 0,
        Normal = 1,
        Data = 2
    };

    struct TextureImportSettings
    {
        TextureType Type = TextureType::Color;
        bool GenerateMipmaps = true;
        TextureWrapMode Wrap = TextureWrapMode::Repeat;
        TextureFilterMode Filter = TextureFilterMode::Linear;

        // Only a colour texture is decoded as sRGB; the others carry linear data.
        bool IsSRGB() const { return Type == TextureType::Color; }
    };

    // One asset's sidecar record: everything about the asset that is not its bytes.
    //
    // The file lives next to the asset as "<asset file name>.meta", so moving or deleting the
    // asset and its record is one operation and a project stays self-describing. The UUID in
    // here is the asset's identity: existing .hscene and .hmaterial references keep working
    // across a rename because only the path changes.
    struct AssetMeta
    {
        UUID ID = UUID::Invalid();
        AssetType Type = AssetType::None;
        // Name of the importer that produced the record. "Native" means the bytes are used as
        // they are; future importers re-encode into a derived artefact and will be named here.
        std::string Importer = "Native";
        TextureImportSettings Texture;

        static AssetMeta MakeDefault(AssetType type);

        // Serializes to the .meta YAML text.
        static std::string Serialize(const AssetMeta& meta);
        // Reads a .meta document. Returns false when the text is not a usable record, and the
        // caller (AssetDatabase) then regenerates the identity rather than failing the whole scan.
        static bool Deserialize(std::string_view text, AssetMeta& out);
    };

    // Sidecar path for an asset path: "Assets/Textures/Grid.png" -> ".../Grid.png.meta".
    std::filesystem::path GetAssetMetaPath(const std::filesystem::path& assetPath);

    // True when the path is a sidecar file rather than an asset.
    bool IsAssetMetaPath(const std::filesystem::path& path);

    // Maps a file extension (lower case, including the dot) to the asset kind it holds, or
    // AssetType::None for content the database does not index. This table is the one place a
    // new asset kind is taught to the pipeline.
    AssetType GetAssetTypeForExtension(std::string_view extension);
}
