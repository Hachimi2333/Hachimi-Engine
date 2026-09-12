#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>

namespace HachimiEngine
{
    class Framebuffer;
    class PostProcessPass;

    // The pair of framebuffers one rendered view needs: an HDR target the scene is drawn
    // into, and an LDR target holding the tone-mapped result the UI displays.
    //
    // The editor viewport, the game panel and the Player each used to build, resize and
    // drive this pair themselves, which meant three copies of the same sequence and three
    // definitions of the editor background color. They share it from here instead.
    class SceneRenderTarget
    {
    public:
        SceneRenderTarget(uint32_t width = 1280, uint32_t height = 720);

        // Recreates the attachments when the size actually changes; anything else is a
        // no-op, so callers can pass the current viewport size every frame.
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        // Binds the HDR target and clears it to the editor background color.
        void BeginScenePass();
        void EndScenePass();

        // Tone maps the HDR scene color into the LDR display target. The exposure travels
        // per call because two views can be resolved in the same frame.
        void Resolve(const PostProcessPass& postProcessPass, float exposure);

        uint32_t GetSceneColorRendererID() const;
        uint32_t GetDisplayColorRendererID() const;

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        Ref<Framebuffer> m_SceneFramebuffer;
        Ref<Framebuffer> m_DisplayFramebuffer;
    };
}
