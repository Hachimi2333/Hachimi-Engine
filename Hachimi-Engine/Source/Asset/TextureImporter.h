#pragma once

#include "Asset/AssetMeta.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Texture.h"

#include <filesystem>

namespace HachimiEngine
{
    // Turns image files into Texture2D resources, applying the import settings recorded in the
    // asset's .meta sidecar. Everything that decides how the bytes are interpreted lives here, so
    // the cache only has to know which asset and which settings it is asking for.
    class TextureImporter
    {
    public:
        // Uploads an image file with the given settings. Returns nullptr when the file cannot be
        // read or decoded.
        static Ref<Texture2D> LoadTexture(const std::filesystem::path& path, const TextureImportSettings& settings);
        // Uploads pixels that were decoded on a worker thread.
        static Ref<Texture2D> UploadDecodedImage(const DecodedImage& image, const TextureImportSettings& settings);
    };
}
