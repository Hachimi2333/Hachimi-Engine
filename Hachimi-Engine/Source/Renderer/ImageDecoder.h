#pragma once

#include "Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace HachimiEngine
{
    // CPU-side image payload, ready to be uploaded to the GPU.
    struct DecodedImage
    {
        // RGBA8 pixels, flipped vertically to match the engine's texture
        // coordinate convention.
        std::vector<uint8_t> Pixels;
        uint32_t Width = 0;
        uint32_t Height = 0;

        bool IsValid() const
        {
            return Width > 0 && Height > 0
                && Pixels.size() == static_cast<size_t>(Width) * Height * 4;
        }
    };

    // Decodes encoded image bytes (PNG/JPG/TGA/BMP) into RGBA8.
    //
    // Decoding is CPU only and touches no OpenGL state, so it is safe to run on a
    // worker thread; only the resulting upload has to happen on the main thread.
    class ImageDecoder
    {
    public:
        static bool DecodeFromMemory(const void* data, size_t size, DecodedImage& outImage);
    };
}
