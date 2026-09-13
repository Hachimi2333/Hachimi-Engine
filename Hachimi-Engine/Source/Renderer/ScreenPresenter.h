#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>

namespace HachimiEngine
{
    class Shader;

    // Draws a finished display image into the window's back buffer.
    //
    // The editor composites its rendered views with ImGui, but a layer that fills the whole
    // window has no UI to do that, so it presents the image itself. The copy is a textured
    // fullscreen triangle rather than a framebuffer blit, because the window is created with
    // multisampling and glBlitFramebuffer requires both framebuffers to have the same sample
    // count. Reusing PostProcessPass would be wrong for the opposite reason: the source is
    // already tone-mapped and gamma-encoded, so it must not be processed again.
    class ScreenPresenter
    {
    public:
        // Creates the program and the empty vertex array; needs a current GL context.
        ScreenPresenter();
        ~ScreenPresenter();

        ScreenPresenter(const ScreenPresenter&) = delete;
        ScreenPresenter& operator=(const ScreenPresenter&) = delete;

        // Copies the source texture over a width x height rectangle of the framebuffer that is
        // currently bound, which for a present is the window's back buffer.
        void Present(uint32_t sourceTexture, uint32_t width, uint32_t height) const;

    private:
        Ref<Shader> m_Shader;
        uint32_t m_VertexArray = 0;
    };
}
