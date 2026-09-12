#pragma once

#include "Renderer/RenderPass.h"

namespace HachimiEngine
{
    // Draws every opaque mesh item with the scene shader, lighting, shadows and IBL.
    //
    // This is the pass that reads the shadow handoff from RenderPassContext, so it must be
    // ordered after the shadow pass.
    class OpaquePass final : public RenderPass
    {
    public:
        std::string_view GetName() const override { return "Opaque"; }

        void Execute(RenderPassContext& context) override;
    };
}
