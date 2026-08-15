#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

namespace HachimiEngine
{
    class Shader;

    // Fullscreen pass that applies tone mapping and gamma encoding to an HDR scene texture.
    class PostProcessPass
    {
    public:
        static void Init();
        static void Shutdown();

        static void Render(uint32_t inputTexture);

        static void SetExposure(float exposure) { s_Exposure = exposure; }
        static float GetExposure() { return s_Exposure; }

    private:
        static Ref<Shader> s_Shader;
        static uint32_t s_VertexArray;
        static float s_Exposure;
    };
}
