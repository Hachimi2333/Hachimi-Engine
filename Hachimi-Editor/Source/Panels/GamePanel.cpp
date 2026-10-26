#include "Panels/GamePanel.h"

#include "Panels/EditorContext.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RendererContext.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"
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

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        if (context.Runtime != nullptr)
        {
            context.Runtime->Resize(width, height);
            context.Runtime->Render(context.Runtime->BuildPrimaryCameraView(aspectRatio));
        }
        else
        {
            m_Target->Resize(width, height);
            m_SceneRenderer->Render(SceneRuntime::BuildPrimaryCameraView(*context.ActiveScene, aspectRatio), *m_Target);
        }
    }

    void GamePanel::Draw(EditorContext& context, bool* open)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Game", open))
        {
            context.GameFocused = false;
            ImGui::End();
            return;
        }

        context.GameFocused = ImGui::IsWindowFocused();
        if (context.GameFocused && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            context.GameInputReleased = false;

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 viewportSize(
            std::max(availableSize.x, 0.0f),
            std::max(availableSize.y, 0.0f));
        context.GameViewportSize = { viewportSize.x, viewportSize.y };

        SceneRenderTarget* target = context.Runtime != nullptr ? &context.Runtime->GetTarget() : m_Target.get();
        if (target != nullptr && target->GetDisplayColorRendererID() != 0 && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(target->GetDisplayColorRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        ImGui::End();
    }
}
