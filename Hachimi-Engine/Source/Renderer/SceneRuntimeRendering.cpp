#include "Scene/SceneRuntime.h"

#include "Renderer/RendererContext.h"
#include "Renderer/SceneRuntimePresentation.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <stdexcept>

namespace HachimiEngine
{
    void SceneRuntime::EnsurePresentation()
    {
        if (m_Presentation != nullptr) return;
        if (m_Renderer == nullptr || !m_Renderer->IsInitialized())
            throw std::logic_error("SceneRuntime rendering requires an initialized RendererContext");
        m_Presentation = CreateScope<SceneRuntimePresentation>();
        m_Presentation->Renderer = CreateScope<SceneRenderer>(*m_Renderer);
        m_Presentation->Target = CreateScope<SceneRenderTarget>(m_Width, m_Height);
        m_Presentation->Debug = CreateScope<DebugDraw>();
    }

    void SceneRuntime::Resize(uint32_t width, uint32_t height)
    {
        m_Width = std::max(width, 1u);
        m_Height = std::max(height, 1u);
        if (m_Scene != nullptr) m_Scene->SetViewportSize(m_Width, m_Height);
        if (m_Presentation != nullptr) m_Presentation->Target->Resize(m_Width, m_Height);
    }

    void SceneRuntime::Render(const RenderView& view)
    {
        if (!IsRunning()) return;
        EnsurePresentation();
        m_Presentation->Renderer->Render(view, *m_Presentation->Target);
    }

    SceneRenderTarget& SceneRuntime::GetTarget() { EnsurePresentation(); return *m_Presentation->Target; }
    DebugDraw& SceneRuntime::GetDebugDraw() { EnsurePresentation(); return *m_Presentation->Debug; }
}
