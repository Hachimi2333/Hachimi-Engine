#pragma once

#include "Renderer/RenderPass.h"

namespace HachimiEngine
{
    // Draws the environment cubemap as a background.
    //
    // Runs before the geometry with depth testing off, so later draws simply overwrite it
    // and the sky needs no depth range trick.
    class SkyboxPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override { return "Skybox"; }

        void Execute(RenderPassContext& context) override;
    };
}
