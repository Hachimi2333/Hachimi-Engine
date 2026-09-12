#pragma once

#include "Core/Base.h"
#include "Renderer/RenderView.h"
#include "Math/Math.h"

#include <string_view>

namespace HachimiEngine
{
    class RendererContext;

    // State shared by every pass of one rendered frame.
    struct RenderPassContext
    {
        RendererContext& Renderers;
        const RenderView& View;

        // Produced by the shadow pass and consumed by the opaque pass, so the two cannot
        // disagree about which shadow volume was rendered.
        Math::Mat4 DirectionalLightViewProjection { 1.0f };
        bool DirectionalShadowEnabled = false;
    };

    // One step of a rendered frame.
    //
    // Passes run in pipeline order and draw into the framebuffer their caller bound. Adding
    // an effect means adding a pass, rather than editing the renderer's draw sequence.
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        // Stable name used by diagnostics and future pass selection.
        virtual std::string_view GetName() const = 0;

        virtual void Execute(RenderPassContext& context) = 0;
    };
}
