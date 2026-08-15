#include "Panels/ViewportPanel.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
#include "Panels/EditorContext.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <ImGuizmo.h>
#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        // Moller-Trumbore ray/triangle intersection. Both triangle windings are accepted.
        bool RayIntersectsTriangle(
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection,
            const glm::vec3& vertex0,
            const glm::vec3& vertex1,
            const glm::vec3& vertex2,
            float& outDistance)
        {
            constexpr float epsilon = 1e-6f;

            const glm::vec3 edge1 = vertex1 - vertex0;
            const glm::vec3 edge2 = vertex2 - vertex0;
            const glm::vec3 pvec = glm::cross(rayDirection, edge2);
            const float determinant = glm::dot(edge1, pvec);

            if (std::fabs(determinant) < epsilon)
            {
                return false;
            }

            const float inverseDeterminant = 1.0f / determinant;
            const glm::vec3 tvec = rayOrigin - vertex0;
            const float u = glm::dot(tvec, pvec) * inverseDeterminant;
            if (u < 0.0f || u > 1.0f)
            {
                return false;
            }

            const glm::vec3 qvec = glm::cross(tvec, edge1);
            const float v = glm::dot(rayDirection, qvec) * inverseDeterminant;
            if (v < 0.0f || u + v > 1.0f)
            {
                return false;
            }

            outDistance = glm::dot(edge2, qvec) * inverseDeterminant;
            return outDistance > epsilon;
        }

        // Finds the closest mesh entity under the mouse cursor using a CPU ray cast.
        Entity PickEntity(const EditorContext& context, const ImVec2& imageMin, const ImVec2& imageMax)
        {
            if (context.ActiveScene == nullptr)
            {
                return {};
            }

            const float imageWidth = imageMax.x - imageMin.x;
            const float imageHeight = imageMax.y - imageMin.y;
            if (imageWidth <= 0.0f || imageHeight <= 0.0f)
            {
                return {};
            }

            const ImVec2 mousePosition = ImGui::GetIO().MousePos;
            const float ndcX = ((mousePosition.x - imageMin.x) / imageWidth) * 2.0f - 1.0f;
            const float ndcY = 1.0f - ((mousePosition.y - imageMin.y) / imageHeight) * 2.0f;

            const glm::mat4 inverseViewProjection = glm::inverse(context.Camera.GetViewProjection());

            const glm::vec4 nearClip = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
            const glm::vec4 farClip = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
            const glm::vec3 rayOrigin = glm::vec3(nearClip.x, nearClip.y, nearClip.z) / nearClip.w;
            const glm::vec3 farPoint = glm::vec3(farClip.x, farClip.y, farClip.z) / farClip.w;
            const glm::vec3 rayDirection = glm::normalize(farPoint - rayOrigin);

            Entity closestEntity;
            float closestDistance = FLT_MAX;

            for (const Entity entity : context.ActiveScene->GetAllEntities())
            {
                if (!entity.HasComponent<MeshComponent>() || !entity.HasComponent<TransformComponent>())
                {
                    continue;
                }

                const auto& mesh = entity.GetComponent<MeshComponent>();
                if (!mesh.Visible || mesh.Mesh == nullptr || mesh.Mesh->GetDrawMode() != MeshDrawMode::Triangles)
                {
                    continue;
                }

                const glm::mat4 inverseWorldTransform = glm::inverse(context.ActiveScene->GetWorldTransform(entity.GetHandle()));
                const glm::vec3 localRayOrigin = glm::vec3(inverseWorldTransform * glm::vec4(rayOrigin, 1.0f));
                const glm::vec3 localRayDirection = glm::normalize(glm::vec3(inverseWorldTransform * glm::vec4(rayDirection, 0.0f)));

                const auto& vertices = mesh.Mesh->GetVertices();
                const auto& indices = mesh.Mesh->GetIndices();

                for (size_t index = 0; index + 2 < indices.size(); index += 3)
                {
                    float distance = 0.0f;
                    if (!RayIntersectsTriangle(
                            localRayOrigin,
                            localRayDirection,
                            vertices[indices[index]].Position,
                            vertices[indices[index + 1]].Position,
                            vertices[indices[index + 2]].Position,
                            distance))
                    {
                        continue;
                    }

                    if (distance < closestDistance)
                    {
                        closestDistance = distance;
                        closestEntity = entity;
                    }
                }
            }

            return closestEntity;
        }

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

    ViewportPanel::ViewportPanel()
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

    void ViewportPanel::RenderScene(EditorContext& context)
    {
        if (context.ActiveScene == nullptr)
        {
            return;
        }

        // Ignore collapsed or otherwise invalid viewport sizes before converting them.
        if (context.ViewportSize.x <= 0.0f || context.ViewportSize.y <= 0.0f)
        {
            return;
        }

        const uint32_t width = static_cast<uint32_t>(context.ViewportSize.x);
        const uint32_t height = static_cast<uint32_t>(context.ViewportSize.y);

        ResizeFramebufferIfNeeded(m_SceneFramebuffer, width, height);
        ResizeFramebufferIfNeeded(m_DisplayFramebuffer, width, height);
        context.Camera.SetViewportSize(width, height);

        m_SceneFramebuffer->Bind();
        // Linear-space clear color matching the previous sRGB editor background.
        RenderCommand::SetClearColor({ 0.00719f, 0.00719f, 0.01002f, 1.0f });
        RenderCommand::Clear();
        context.ActiveScene->OnRender(context.Camera);
        m_SceneFramebuffer->Unbind();

        m_DisplayFramebuffer->Bind();
        RenderCommand::Clear();
        PostProcessPass::Render(m_SceneFramebuffer->GetColorAttachmentRendererID());
        m_DisplayFramebuffer->Unbind();
    }

    void ViewportPanel::Draw(EditorContext& context)
    {
        // Keep the viewport usable even when a stale layout saved a collapsed size.
        ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Viewport");

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 viewportSize(
            std::max(availableSize.x, 0.0f),
            std::max(availableSize.y, 0.0f));
        context.ViewportSize = { viewportSize.x, viewportSize.y };
        context.ViewportHovered = ImGui::IsWindowHovered();
        context.ViewportFocused = ImGui::IsWindowFocused();

        ImVec2 imageMin(0.0f, 0.0f);
        ImVec2 imageMax(0.0f, 0.0f);
        bool hasViewportImage = false;

        if (m_DisplayFramebuffer->GetColorAttachmentRendererID() != 0 && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
        {
            // UVs are flipped vertically for the OpenGL framebuffer texture.
            ImGui::Image(
                static_cast<ImTextureID>(m_DisplayFramebuffer->GetColorAttachmentRendererID()),
                viewportSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));

            imageMin = ImGui::GetItemRectMin();
            imageMax = ImGui::GetItemRectMax();
            hasViewportImage = true;

            // Left-clicking the viewport image selects the mesh entity under the cursor.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)
                && !Input::IsKeyPressed(Key::LeftAlt)
                && !ImGuizmo::IsOver()
                && !ImGuizmo::IsUsingAny())
            {
                context.SelectedEntity = PickEntity(context, imageMin, imageMax);
            }
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

        if (hasViewportImage)
        {
            ManipulateSelectedEntity(context, imageMin, imageMax);
        }

        ImGui::End();
    }

    void ViewportPanel::ManipulateSelectedEntity(EditorContext& context, const ImVec2& imageMin, const ImVec2& imageMax)
    {
        if (!context.SelectedEntity || context.ActiveScene == nullptr)
        {
            return;
        }

        Entity selected = context.SelectedEntity;
        if (!selected.HasComponent<TransformComponent>())
        {
            return;
        }

        glm::mat4 worldTransform = context.ActiveScene->GetWorldTransform(selected.GetHandle());
        glm::mat4 parentWorldTransform(1.0f);
        bool hasParent = false;

        if (selected.HasComponent<RelationshipComponent>())
        {
            const auto& relationship = selected.GetComponent<RelationshipComponent>();
            if (relationship.Parent != UUID::Invalid())
            {
                const Entity parent = context.ActiveScene->GetEntityByUUID(relationship.Parent);
                if (parent)
                {
                    parentWorldTransform = context.ActiveScene->GetWorldTransform(parent.GetHandle());
                    hasParent = true;
                }
            }
        }

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        // Match the gizmo viewport to the framebuffer image, not the whole docking window.
        ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);

        const glm::mat4& view = context.Camera.GetViewMatrix();
        const glm::mat4& projection = context.Camera.GetProjection();

        if (ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            context.GizmoOperation,
            ImGuizmo::LOCAL,
            glm::value_ptr(worldTransform)))
        {
            // Convert the manipulated world transform back into parent-local space.
            glm::mat4 localTransform = hasParent
                ? glm::inverse(parentWorldTransform) * worldTransform
                : worldTransform;

            glm::vec3 translation(0.0f);
            glm::vec3 rotationRadians(0.0f);
            glm::vec3 scale(1.0f);
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), glm::value_ptr(translation), glm::value_ptr(rotationRadians), glm::value_ptr(scale));

            // Decompose rotation with GLM so the Euler values round-trip through
            // TransformComponent::GetTransform() without the ImGuizmo Euler convention mismatch.
            glm::mat3 rotationMatrix(localTransform);
            constexpr float minimumScale = 1e-6f;
            rotationMatrix[0] = scale.x > minimumScale ? rotationMatrix[0] / scale.x : glm::vec3(1.0f, 0.0f, 0.0f);
            rotationMatrix[1] = scale.y > minimumScale ? rotationMatrix[1] / scale.y : glm::vec3(0.0f, 1.0f, 0.0f);
            rotationMatrix[2] = scale.z > minimumScale ? rotationMatrix[2] / scale.z : glm::vec3(0.0f, 0.0f, 1.0f);
            const glm::quat rotation = glm::quat_cast(rotationMatrix);
            rotationRadians = glm::eulerAngles(rotation);

            auto& transform = selected.Transform();
            transform.Position = translation;
            transform.Rotation = glm::degrees(rotationRadians);
            transform.Scale = scale;
        }
    }
}
