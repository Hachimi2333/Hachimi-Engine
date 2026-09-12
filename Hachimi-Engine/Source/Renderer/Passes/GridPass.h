#pragma once

#include "Renderer/RenderPass.h"

namespace HachimiEngine
{
    // Draws the editor ground grid. Runtime views leave RenderView::DrawGrid off.
    class GridPass final : public RenderPass
    {
    public:
        std::string_view GetName() const override { return "Grid"; }

        void Execute(RenderPassContext& context) override;
    };
}
