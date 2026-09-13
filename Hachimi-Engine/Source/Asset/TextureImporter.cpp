#include "Asset/TextureImporter.h"

#include "Core/Log.h"
#include "Renderer/ImageDecoder.h"
#include "Utils/VirtualFileSystem.h"

#include <vector>

namespace HachimiEngine
{
    namespace
    {
        TextureSpecification MakeSpecification(const TextureImportSettings& settings)
        {
            TextureSpecification specification;
            specification.SRGB = settings.IsSRGB();
            specification.GenerateMips = settings.GenerateMipmaps;

            switch (settings.Wrap)
            {
                case TextureWrapMode::Clamp: specification.Address = TextureAddressMode::Clamp; break;
                case TextureWrapMode::MirroredRepeat: specification.Address = TextureAddressMode::MirroredRepeat; break;
                case TextureWrapMode::Repeat:
                default: specification.Address = TextureAddressMode::Repeat; break;
            }

            switch (settings.Filter)
            {
                case TextureFilterMode::Nearest: specification.Filter = TextureSamplingFilter::Nearest; break;
                case TextureFilterMode::Linear:
                default: specification.Filter = TextureSamplingFilter::Linear; break;
            }

            return specification;
        }
    }

    Ref<Texture2D> TextureImporter::UploadDecodedImage(const DecodedImage& image, const TextureImportSettings& settings)
    {
        return Texture2D::Create(MakeSpecification(settings), image);
    }

    Ref<Texture2D> TextureImporter::LoadTexture(const std::filesystem::path& path, const TextureImportSettings& settings)
    {
        // Reading goes through the virtual file system and decoding is CPU-only, so this works
        // identically for a loose file and for a texture inside a game package.
        std::vector<uint8_t> bytes;
        DecodedImage image;
        if (!VirtualFileSystem::ReadBinaryFile(path, bytes)
            || !ImageDecoder::DecodeFromMemory(bytes.data(), bytes.size(), image))
        {
            HE_CORE_ERROR("Failed to read or decode texture: {}", path.string());
            return nullptr;
        }

        return UploadDecodedImage(image, settings);
    }
}
