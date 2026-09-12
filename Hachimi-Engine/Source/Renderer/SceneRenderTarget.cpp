#include "Renderer/SceneRenderTarget.h"

#include "Renderer/FrameBuffer.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    namespace
    {
        // Linear-space equivalent of the sRGB editor background, so the viewport and the
        // game panel clear to the same visible color as the surrounding ImGui chrome.
        const Math::Vec4 EditorBackgroundColor { 0.00719f, 0.00719f, 0.01002f, 1.0f };
    }

    SceneRenderTarget::SceneRenderTarget(uint32_t width, uint32_t height)
        : m_Width(width > 0 ? width : 1)
        , m_Height(height > 0 ? height : 1)
    {
        FramebufferSpecification sceneSpecification;
        sceneSpecification.Width = m_Width;
        sceneSpecification.Height = m_Height;
        sceneSpecification.ColorFormat = FramebufferColorFormat::RGBA16F;
        m_SceneFramebuffer = Framebuffer::Create(sceneSpecification);

        FramebufferSpecification displaySpecification;
        displaySpecification.Width = m_Width;
        displaySpecification.Height = m_Height;
        m_DisplayFramebuffer = Framebuffer::Create(displaySpecification);
    }

    void SceneRenderTarget::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (width == m_Width && height == m_Height))
        {
            return;
        }

        m_Width = width;
        m_Height = height;
        m_SceneFramebuffer->Resize(width, height);
        m_DisplayFramebuffer->Resize(width, height);
    }

    void SceneRenderTarget::BeginScenePass()
    {
        m_SceneFramebuffer->Bind();
        RenderCommand::SetClearColor(EditorBackgroundColor);
        RenderCommand::Clear();
    }

    void SceneRenderTarget::EndScenePass()
    {
        m_SceneFramebuffer->Unbind();
    }

    void SceneRenderTarget::Resolve(const PostProcessPass& postProcessPass, float exposure)
    {
        m_DisplayFramebuffer->Bind();
        RenderCommand::Clear();
        postProcessPass.Render(m_SceneFramebuffer->GetColorAttachmentRendererID(), exposure);
        m_DisplayFramebuffer->Unbind();
    }

    uint32_t SceneRenderTarget::GetSceneColorRendererID() const
    {
        return m_SceneFramebuffer->GetColorAttachmentRendererID();
    }

    uint32_t SceneRenderTarget::GetDisplayColorRendererID() const
    {
        return m_DisplayFramebuffer->GetColorAttachmentRendererID();
    }
}
