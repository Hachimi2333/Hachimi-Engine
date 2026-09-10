#include "Renderer/ImageDecoder.h"

#include "Core/Log.h"

#include <stb_image.h>

#include <limits>

namespace HachimiEngine
{
    bool ImageDecoder::DecodeFromMemory(const void* data, size_t size, DecodedImage& outImage)
    {
        outImage = {};

        if (data == nullptr || size == 0 || size > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return false;
        }

        // Engine textures are sampled with a top-left UV origin, so decoding
        // flips rows. Kept here so the synchronous and asynchronous paths cannot
        // diverge.
        stbi_set_flip_vertically_on_load(1);

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            static_cast<const stbi_uc*>(data), static_cast<int>(size), &width, &height, &channels, 4);

        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            HE_CORE_ERROR("Failed to decode image data: {}", stbi_failure_reason() ? stbi_failure_reason() : "unknown");
            if (pixels != nullptr)
            {
                stbi_image_free(pixels);
            }
            return false;
        }

        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        outImage.Width = static_cast<uint32_t>(width);
        outImage.Height = static_cast<uint32_t>(height);
        outImage.Pixels.assign(pixels, pixels + pixelCount);
        stbi_image_free(pixels);

        return outImage.IsValid();
    }
}
