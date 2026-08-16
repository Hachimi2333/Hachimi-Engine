#include "Panels/GamePanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <glad/gl.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>

namespace HachimiEngine
{
    namespace
    {
        void ResizeFramebufferIfNeeded(const Ref<Framebuffer>& framebuffer, uint32_t width, uint32_t height)
        {
            if (framebuffer == nullptr)
            {
                return;
            }

            const auto& specification = framebuffer->GetSpecification();
            if (width > 0 && height > 0 && (width != specification.Width || height != specification.Height))
            {
                framebuffer->Resize(width, height);
            }
        }
    }

    GamePanel::GamePanel()
    {
        FramebufferSpecification sceneSpecification;
        sceneSpecification.Width = 1280;
        sceneSpecification.Height = 720;
        sceneSpecification.ColorFormat = FramebufferColorFormat::RGBA16F;
        m_SceneFramebuffer = Framebuffer::Create(sceneSpecification);

        FramebufferSpecification displaySpecification;
        displaySpecification.Width = 1280;
        displaySpecification.Height = 720;
        m_DisplayFramebuffer = Framebuffer::Create(displaySpecification);
    }

    void GamePanel::RenderScene(EditorContext& context)
    {
        if (context.ActiveScene == nullptr)
        {
            return;
        }

        if (context.GameViewportSize.x <= 0.0f || context.GameViewportSize.y <= 0.0f)
        {
            return;
        }

        const uint32_t width = static_cast<uint32_t>(context.GameViewportSize.x);
        const uint32_t height = static_cast<uint32_t>(context.GameViewportSize.y);

        ResizeFramebufferIfNeeded(m_SceneFramebuffer, width, height);
        ResizeFramebufferIfNeeded(m_DisplayFramebuffer, width, height);

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        Math::Mat4 viewMatrix(1.0f);
        Math::Mat4 projectionMatrix(1.0f);
        Math::Vec3 cameraPosition(0.0f);

        // Render from the primary scene camera, falling back to the editor camera.
        const Entity primaryCamera = context.ActiveScene->GetPrimaryCameraEntity();
        if (primaryCamera
            && primaryCamera.HasComponent<TransformComponent>()
            && primaryCamera.HasComponent<CameraComponent>())
        {
            const auto& cameraComponent = primaryCamera.GetComponent<CameraComponent>();
            const Math::Mat4 cameraWorld = context.ActiveScene->GetWorldTransform(primaryCamera.GetHandle());
            viewMatrix = Math::Inverse(cameraWorld);
            cameraPosition = Math::Vec3(cameraWorld[3].x, cameraWorld[3].y, cameraWorld[3].z);
            projectionMatrix = Math::Perspective(
                Math::Radians(cameraComponent.FieldOfView),
                aspectRatio,
                cameraComponent.NearClip,
                cameraComponent.FarClip);
        }
        else
        {
            viewMatrix = context.Camera.GetViewMatrix();
            cameraPosition = context.Camera.GetPosition();
            projectionMatrix = Math::Perspective(Math::Radians(context.Camera.GetFieldOfView()), aspectRatio, 0.1f, 1000.0f);
        }

        m_SceneFramebuffer->Bind();
        // Linear-space clear color matching the previous sRGB editor background.
        RenderCommand::SetClearColor({ 0.00719f, 0.00719f, 0.01002f, 1.0f });
        RenderCommand::Clear();
        context.ActiveScene->OnRender(viewMatrix, projectionMatrix, cameraPosition);
        m_SceneFramebuffer->Unbind();

        m_DisplayFramebuffer->Bind();
        RenderCommand::Clear();
        PostProcessPass::Render(m_SceneFramebuffer->GetColorAttachmentRendererID());
        m_DisplayFramebuffer->Unbind();
    }

    void GamePanel::Draw(EditorContext& context)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Game");

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 viewportSize(
            std::max(availableSize.x, 0.0f),
            std::max(availableSize.y, 0.0f));
        context.GameViewportSize = { viewportSize.x, viewportSize.y };

        if (m_DisplayFramebuffer->GetColorAttachmentRendererID() != 0 && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(m_DisplayFramebuffer->GetColorAttachmentRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        ImGui::End();
    }
}
