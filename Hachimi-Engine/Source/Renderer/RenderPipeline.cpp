#include "Renderer/RenderPipeline.h"

namespace HachimiEngine
{
    void RenderPipeline::Execute(RenderPassContext& context) const
    {
        for (const Scope<RenderPass>& pass : m_Passes)
        {
            if (pass != nullptr)
            {
                pass->Execute(context);
            }
        }
    }
}
