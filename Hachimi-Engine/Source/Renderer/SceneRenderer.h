#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Lighting.h"
#include "Renderer/RenderView.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    class RendererContext;

    // Draws one RenderView per frame into the currently bound framebuffer.
    //
    // The renderer holds the frame state it needs to draw (matrices, light transforms,
    // the shadow pass flag) as instance members, so the editor viewport, the game panel
    // and the Player each own one and cannot disturb each other. Everything that outlives
    // a frame - shaders, built-in geometry and the GPU mesh cache - lives in the shared
    // RendererContext it is constructed with.
    class SceneRenderer
    {
    public:
        explicit SceneRenderer(RendererContext& context);

        SceneRenderer(const SceneRenderer&) = delete;
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        // Renders the view into whatever framebuffer is bound. Callers own the target.
        void Render(const RenderView& view);

    private:
        // Per-frame values derived from the view, shared by the passes below.
        struct FrameState
        {
            Math::Mat4 ViewProjection { 1.0f };
            Math::Mat4 View { 1.0f };
            Math::Mat4 Projection { 1.0f };
            Math::Vec3 CameraPosition { 0.0f };
            Math::Vec3 CameraForward { 0.0f, 0.0f, -1.0f };
            Math::Mat4 DirectionalLightViewProjection { 1.0f };
            bool DirectionalShadowEnabled = false;
        };

        void DrawDirectionalShadowPass(const RenderView& view, const FrameState& frame);
        void DrawSkybox(const RenderView& view, const FrameState& frame);
        void DrawGrid(const FrameState& frame);
        void DrawItems(const RenderView& view, const FrameState& frame);

        void SubmitMesh(const RenderView& view, const FrameState& frame, const RenderItem& item);

        void UploadLighting(const Ref<Shader>& shader, const FrameState& frame, const LightingEnvironment& lighting);

        Math::Mat4 CalculateDirectionalLightViewProjection(const FrameState& frame, const DirectionalLight& light) const;

    private:
        RendererContext& m_Context;
    };
}
