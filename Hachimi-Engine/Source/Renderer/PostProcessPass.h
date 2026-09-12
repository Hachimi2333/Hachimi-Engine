#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>

namespace HachimiEngine
{
    class Shader;

    // Fullscreen pass that applies tone mapping and gamma encoding to an HDR scene texture.
    //
    // The exposure is passed in per call rather than stored, so two views rendered in the
    // same frame (editor viewport and game panel) cannot overwrite each other's setting.
    class PostProcessPass
    {
    public:
        // Creates the program and the empty vertex array; needs a current GL context.
        PostProcessPass();
        ~PostProcessPass();

        PostProcessPass(const PostProcessPass&) = delete;
        PostProcessPass& operator=(const PostProcessPass&) = delete;

        // Draws the source texture into the currently bound framebuffer.
        void Render(uint32_t inputTexture, float exposure);

    private:
        Ref<Shader> m_Shader;
        uint32_t m_VertexArray = 0;
    };
}
