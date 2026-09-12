#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/RenderPipeline.h"
#include "Renderer/RenderView.h"

#include <functional>

namespace HachimiEngine
{
    class RendererContext;
    class SceneRenderTarget;

    // Renders one frame of a scene into a SceneRenderTarget.
    //
    // Frame state lives in the pass context rather than in this object, and the draw order
    // lives in the pipeline, so the editor viewport, the game panel and the Player each own
    // one of these without being able to disturb the others. Everything that outlives a
    // frame - shaders, built-in geometry, the GPU mesh cache, the shadow map - belongs to
    // the shared RendererContext this is constructed with.
    class SceneRenderer
    {
    public:
        explicit SceneRenderer(RendererContext& context);

        SceneRenderer(const SceneRenderer&) = delete;
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        // Renders the view: the scene passes into the target's HDR buffer, then an optional
        // overlay (editor gizmos, which must land in the HDR buffer before tone mapping),
        // then tone mapping into the display buffer.
        void Render(const RenderView& view, SceneRenderTarget& target, const std::function<void()>& drawOverlay = {});

        RenderPipeline& GetPipeline() { return m_Pipeline; }
        const RenderPipeline& GetPipeline() const { return m_Pipeline; }

    private:
        void BuildDefaultPipeline();

    private:
        RendererContext& m_Context;
        RenderPipeline m_Pipeline;
    };
}
