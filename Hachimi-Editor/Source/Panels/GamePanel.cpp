#include "Panels/GamePanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cfloat>

namespace HachimiEngine
{
    GamePanel::GamePanel()
    {
        FramebufferSpecification specification;
        specification.Width = 1280;
        specification.Height = 720;
        m_Framebuffer = Framebuffer::Create(specification);
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

        const auto& specification = m_Framebuffer->GetSpecification();
        const uint32_t width = static_cast<uint32_t>(context.GameViewportSize.x);
        const uint32_t height = static_cast<uint32_t>(context.GameViewportSize.y);

        if (width > 0 && height > 0 && (width != specification.Width || height != specification.Height))
        {
            m_Framebuffer->Resize(width, height);
        }

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        glm::mat4 viewMatrix(1.0f);
        glm::mat4 projectionMatrix(1.0f);
        glm::vec3 cameraPosition(0.0f);

        // Render from the primary scene camera, falling back to the editor camera.
        const Entity primaryCamera = context.ActiveScene->GetPrimaryCameraEntity();
        if (primaryCamera
            && primaryCamera.HasComponent<TransformComponent>()
            && primaryCamera.HasComponent<CameraComponent>())
        {
            const auto& cameraComponent = primaryCamera.GetComponent<CameraComponent>();
            const glm::mat4 cameraWorld = context.ActiveScene->GetWorldTransform(primaryCamera.GetHandle());
            viewMatrix = glm::inverse(cameraWorld);
            cameraPosition = glm::vec3(cameraWorld[3].x, cameraWorld[3].y, cameraWorld[3].z);
            projectionMatrix = glm::perspective(
                glm::radians(cameraComponent.FieldOfView),
                aspectRatio,
                cameraComponent.NearClip,
                cameraComponent.FarClip);
        }
        else
        {
            viewMatrix = context.Camera.GetViewMatrix();
            cameraPosition = context.Camera.GetPosition();
            projectionMatrix = glm::perspective(glm::radians(context.Camera.GetFieldOfView()), aspectRatio, 0.1f, 1000.0f);
        }

        m_Framebuffer->Bind();
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.10f, 1.0f });
        RenderCommand::Clear();
        context.ActiveScene->OnRender(viewMatrix, projectionMatrix, cameraPosition);
        m_Framebuffer->Unbind();
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

        if (m_Framebuffer->GetColorAttachmentRendererID() != 0 && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(m_Framebuffer->GetColorAttachmentRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        ImGui::End();
    }
}
