#include "Renderer/SceneRenderer.h"

#include "Core/Assert.h"
#include "Renderer/Passes/DirectionalShadowPass.h"
#include "Renderer/Passes/GridPass.h"
#include "Renderer/Passes/OpaquePass.h"
#include "Renderer/Passes/SkyboxPass.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RendererContext.h"
#include "Renderer/SceneRenderTarget.h"

namespace HachimiEngine
{
    SceneRenderer::SceneRenderer(RendererContext& context)
        : m_Context(context)
    {
        BuildDefaultPipeline();
    }

    void SceneRenderer::BuildDefaultPipeline()
    {
        // Order matters: the shadow map is rendered first because the opaque pass samples
        // it, the sky is drawn without depth testing so the geometry can overwrite it, and
        // the grid sits between them so it is occluded by geometry but drawn over the sky.
        m_Pipeline.AddPass<DirectionalShadowPass>();
        m_Pipeline.AddPass<SkyboxPass>();
        m_Pipeline.AddPass<GridPass>();
        m_Pipeline.AddPass<OpaquePass>();
    }

    void SceneRenderer::Render(const RenderView& view, SceneRenderTarget& target, const std::function<void()>& drawOverlay)
    {
        HE_CORE_ASSERT(m_Context.IsInitialized());

        target.BeginScenePass();

        RenderPassContext passContext { m_Context, view };
        m_Pipeline.Execute(passContext);

        if (drawOverlay)
        {
            drawOverlay();
        }

        target.EndScenePass();

        target.Resolve(m_Context.GetPostProcessPass(), view.Environment.Exposure);
    }
}
