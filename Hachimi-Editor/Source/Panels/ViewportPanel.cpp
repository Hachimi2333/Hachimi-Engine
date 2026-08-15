#include "Panels/ViewportPanel.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
#include "Panels/EditorContext.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <ImGuizmo.h>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace HachimiEngine
{
    ViewportPanel::ViewportPanel()
    {
        FramebufferSpecification specification;
        specification.Width = 1280;
        specification.Height = 720;
        m_Framebuffer = Framebuffer::Create(specification);
    }

    void ViewportPanel::RenderScene(EditorContext& context)
    {
        if (context.Scene == nullptr)
        {
            return;
        }

        const auto& specification = m_Framebuffer->GetSpecification();
        const uint32_t width = static_cast<uint32_t>(context.ViewportSize.x);
        const uint32_t height = static_cast<uint32_t>(context.ViewportSize.y);

        if (width > 0 && height > 0 && (width != specification.Width || height != specification.Height))
        {
            m_Framebuffer->Resize(width, height);
            context.Camera.SetViewportSize(width, height);
        }

        m_Framebuffer->Bind();
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.10f, 1.0f });
        RenderCommand::Clear();
        context.Scene->OnRender(context.Camera);
        m_Framebuffer->Unbind();
    }

    void ViewportPanel::Draw(EditorContext& context)
    {
        ImGui::Begin("Viewport");
        DrawGizmoToolbar(context);

        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        context.ViewportSize = { viewportSize.x, viewportSize.y };
        context.ViewportHovered = ImGui::IsWindowHovered();
        context.ViewportFocused = ImGui::IsWindowFocused();

        if (m_Framebuffer->GetColorAttachmentRendererID() != 0)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(m_Framebuffer->GetColorAttachmentRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (context.ViewportHovered && !ImGuizmo::IsUsingViewManipulate())
        {
            if (io.MouseWheel != 0.0f)
            {
                context.Camera.OnMouseScroll(io.MouseWheel);
            }

            const glm::vec2 mouseDelta(io.MouseDelta.x, io.MouseDelta.y);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            {
                context.Camera.OnMouseDrag(mouseDelta, Mouse::ButtonRight);
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                context.Camera.OnMouseDrag(mouseDelta, Mouse::ButtonMiddle);
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && Input::IsKeyPressed(Key::LeftAlt))
            {
                context.Camera.OnMouseDrag(mouseDelta, Mouse::ButtonLeft);
            }
        }

        ManipulateSelectedEntity(context);
        ImGui::End();
    }

    void ViewportPanel::DrawGizmoToolbar(EditorContext& context)
    {
        ImGui::TextUnformatted("Gizmo:");

        ImGui::SameLine();
        if (ImGui::Button("Translate"))
        {
            context.GizmoOperation = ImGuizmo::TRANSLATE;
        }

        ImGui::SameLine();
        if (ImGui::Button("Rotate"))
        {
            context.GizmoOperation = ImGuizmo::ROTATE;
        }

        ImGui::SameLine();
        if (ImGui::Button("Scale"))
        {
            context.GizmoOperation = ImGuizmo::SCALE;
        }

        ImGui::Separator();
    }

    void ViewportPanel::ManipulateSelectedEntity(EditorContext& context)
    {
        if (!context.SelectedEntity || context.Scene == nullptr)
        {
            return;
        }

        Entity selected = context.SelectedEntity;
        if (!selected.HasComponent<TransformComponent>())
        {
            return;
        }

        glm::mat4 worldTransform = context.Scene->GetWorldTransform(selected.GetHandle());
        glm::mat4 localTransform = worldTransform;

        if (selected.HasComponent<RelationshipComponent>())
        {
            const auto& relationship = selected.GetComponent<RelationshipComponent>();
            if (relationship.Parent != UUID::Invalid())
            {
                const Entity parent = context.Scene->GetEntityByUUID(relationship.Parent);
                if (parent)
                {
                    const glm::mat4 parentWorld = context.Scene->GetWorldTransform(parent.GetHandle());
                    localTransform = glm::inverse(parentWorld) * worldTransform;
                }
            }
        }

        const ImVec2 windowPosition = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(windowPosition.x, windowPosition.y, windowSize.x, windowSize.y);

        const glm::mat4& view = context.Camera.GetViewMatrix();
        const glm::mat4& projection = context.Camera.GetProjection();

        if (ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            context.GizmoOperation,
            ImGuizmo::LOCAL,
            glm::value_ptr(localTransform)))
        {
            glm::vec3 translation(0.0f);
            glm::vec3 rotationRadians(0.0f);
            glm::vec3 scale(1.0f);
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), glm::value_ptr(translation), glm::value_ptr(rotationRadians), glm::value_ptr(scale));

            auto& transform = selected.Transform();
            transform.Position = translation;
            transform.Rotation = glm::degrees(rotationRadians);
            transform.Scale = scale;
        }
    }
}
