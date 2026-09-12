#include "Panels/GamePanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RendererContext.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <glad/gl.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>

namespace HachimiEngine
{
    GamePanel::GamePanel() = default;

    void GamePanel::Init(RendererContext& rendererContext)
    {
        m_Renderer = &rendererContext;
        m_SceneRenderer = CreateScope<SceneRenderer>(rendererContext);
        m_Target = CreateScope<SceneRenderTarget>();
    }

    GamePanel::~GamePanel() = default;

    void GamePanel::RenderScene(EditorContext& context)
    {
        if (context.ActiveScene == nullptr || m_SceneRenderer == nullptr)
        {
            return;
        }

        if (context.GameViewportSize.x <= 0.0f || context.GameViewportSize.y <= 0.0f)
        {
            return;
        }

        const uint32_t width = static_cast<uint32_t>(context.GameViewportSize.x);
        const uint32_t height = static_cast<uint32_t>(context.GameViewportSize.y);

        m_Target->Resize(width, height);

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        SceneRenderDesc desc;
        desc.DrawGrid = false;

        // Render from the primary scene camera, falling back to the editor camera.
        const Entity primaryCamera = context.ActiveScene->GetPrimaryCameraEntity();
        if (primaryCamera
            && primaryCamera.HasComponent<TransformComponent>()
            && primaryCamera.HasComponent<CameraComponent>())
        {
            const auto& cameraComponent = primaryCamera.GetComponent<CameraComponent>();
            const Math::Mat4 cameraWorld = context.ActiveScene->GetWorldTransform(primaryCamera.GetHandle());
            desc.View = Math::Inverse(cameraWorld);
            desc.CameraPosition = Math::Vec3(cameraWorld[3].x, cameraWorld[3].y, cameraWorld[3].z);
            desc.Projection = Math::Perspective(
                Math::Radians(cameraComponent.FieldOfView),
                aspectRatio,
                cameraComponent.NearClip,
                cameraComponent.FarClip);
        }
        else
        {
            desc.View = context.Camera.GetViewMatrix();
            desc.CameraPosition = context.Camera.GetPosition();
            desc.Projection = Math::Perspective(Math::Radians(context.Camera.GetFieldOfView()), aspectRatio, 0.1f, 1000.0f);
        }

        const RenderView view = context.ActiveScene->BuildRenderView(desc);

        m_SceneRenderer->Render(view, *m_Target);
    }

    void GamePanel::Draw(EditorContext& context)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Game"))
        {
            ImGui::End();
            return;
        }

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 viewportSize(
            std::max(availableSize.x, 0.0f),
            std::max(availableSize.y, 0.0f));
        context.GameViewportSize = { viewportSize.x, viewportSize.y };

        if (m_Target != nullptr && m_Target->GetDisplayColorRendererID() != 0 && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(m_Target->GetDisplayColorRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        ImGui::End();
    }
}
