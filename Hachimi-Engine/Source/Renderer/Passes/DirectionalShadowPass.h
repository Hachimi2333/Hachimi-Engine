#pragma once

#include "Renderer/RenderPass.h"

namespace HachimiEngine
{
    // Renders the directional light's shadow volume into the shadow map.
    //
    // Runs first so the opaque pass can sample the map. When the scene has no shadow
    // casting directional light the pass clears the shared flag and draws nothing, which
    // is what makes the opaque pass skip the shadow lookup.
    class DirectionalShadowPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override { return "DirectionalShadow"; }

        void Execute(RenderPassContext& context) override;
    };
}
