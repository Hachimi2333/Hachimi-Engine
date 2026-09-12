#include "Viewport/SelectionIndicators.h"

#include "Panels/EditorContext.h"
#include "Renderer/DebugDraw.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        const Math::Vec4 CameraIndicatorColor { 0.15f, 0.85f, 1.0f, 0.95f };
        const Math::Vec4 PointLightIndicatorColor { 1.0f, 0.80f, 0.25f, 0.90f };
        const Math::Vec4 DirectionalLightIndicatorColor { 1.0f, 0.86f, 0.29f, 0.95f };

        Math::Vec3 WorldPosition(const Math::Mat4& worldTransform)
        {
            return { worldTransform[3].x, worldTransform[3].y, worldTransform[3].z };
        }

        Math::Vec3 WorldDirection(const Math::Mat4& worldTransform, const Math::Vec3& localDirection)
        {
            const Math::Vec4 worldDirection = worldTransform * Math::Vec4(localDirection, 0.0f);
            return Math::Normalize(Math::Vec3(worldDirection.x, worldDirection.y, worldDirection.z));
        }

        void DrawWorldRectangle(
            DebugDraw& debugDraw,
            const Math::Vec3& topLeft,
            const Math::Vec3& topRight,
            const Math::Vec3& bottomRight,
            const Math::Vec3& bottomLeft,
            const Math::Vec4& color)
        {
            debugDraw.DrawLine(topLeft, topRight, color);
            debugDraw.DrawLine(topRight, bottomRight, color);
            debugDraw.DrawLine(bottomRight, bottomLeft, color);
            debugDraw.DrawLine(bottomLeft, topLeft, color);
        }

        void DrawCameraIndicator(DebugDraw& debugDraw, const EditorContext& context, const Math::Mat4& worldTransform)
        {
            const auto& camera = context.SelectedEntity.GetComponent<CameraComponent>();

            const Math::Vec3 position = WorldPosition(worldTransform);
            const Math::Vec3 right = Math::Normalize(Math::Vec3(worldTransform[0]));
            const Math::Vec3 up = Math::Normalize(Math::Vec3(worldTransform[1]));
            const Math::Vec3 forward = Math::Normalize(-Math::Vec3(worldTransform[2]));

            const float nearClip = std::max(camera.NearClip, 0.01f);
            const float farClip = std::max(camera.FarClip, nearClip + 0.1f);
            const float aspectRatio = context.ViewportSize.y > 0.0f
                ? context.ViewportSize.x / context.ViewportSize.y
                : 1.0f;
            const float tanHalfFov = std::tan(Math::Radians(std::max(camera.FieldOfView, 1.0f)) * 0.5f);

            const Math::Vec3 nearCenter = position + forward * nearClip;
            const Math::Vec3 farCenter = position + forward * farClip;
            const float nearHalfHeight = nearClip * tanHalfFov;
            const float nearHalfWidth = nearHalfHeight * aspectRatio;
            const float farHalfHeight = farClip * tanHalfFov;
            const float farHalfWidth = farHalfHeight * aspectRatio;

            const Math::Vec3 nearTopLeft = nearCenter + up * nearHalfHeight - right * nearHalfWidth;
            const Math::Vec3 nearTopRight = nearCenter + up * nearHalfHeight + right * nearHalfWidth;
            const Math::Vec3 nearBottomRight = nearCenter - up * nearHalfHeight + right * nearHalfWidth;
            const Math::Vec3 nearBottomLeft = nearCenter - up * nearHalfHeight - right * nearHalfWidth;

            const Math::Vec3 farTopLeft = farCenter + up * farHalfHeight - right * farHalfWidth;
            const Math::Vec3 farTopRight = farCenter + up * farHalfHeight + right * farHalfWidth;
            const Math::Vec3 farBottomRight = farCenter - up * farHalfHeight + right * farHalfWidth;
            const Math::Vec3 farBottomLeft = farCenter - up * farHalfHeight - right * farHalfWidth;

            DrawWorldRectangle(debugDraw, nearTopLeft, nearTopRight, nearBottomRight, nearBottomLeft, CameraIndicatorColor);
            DrawWorldRectangle(debugDraw, farTopLeft, farTopRight, farBottomRight, farBottomLeft, CameraIndicatorColor);

            debugDraw.DrawLine(nearTopLeft, farTopLeft, CameraIndicatorColor);
            debugDraw.DrawLine(nearTopRight, farTopRight, CameraIndicatorColor);
            debugDraw.DrawLine(nearBottomRight, farBottomRight, CameraIndicatorColor);
            debugDraw.DrawLine(nearBottomLeft, farBottomLeft, CameraIndicatorColor);

            // Emphasize the camera's forward axis inside the frustum.
            debugDraw.DrawLine(position, farCenter, CameraIndicatorColor);
        }

        void DrawPointLightIndicator(DebugDraw& debugDraw, const LightComponent& light, const Math::Mat4& worldTransform)
        {
            const Math::Vec3 position = WorldPosition(worldTransform);
            debugDraw.DrawSphere(position, std::max(0.01f, light.Range), PointLightIndicatorColor, 48);
        }

        void DrawDirectionalLightIndicator(DebugDraw& debugDraw, const EditorContext& context, const Math::Mat4& worldTransform)
        {
            const Math::Vec3 position = WorldPosition(worldTransform);
            const Math::Vec3 direction = WorldDirection(worldTransform, Math::Vec3(0.0f, 0.0f, -1.0f));

            // Keep the arrow readable both when the camera is close and when it is far away.
            const float length = std::clamp(context.Camera.GetDistance() * 0.15f, 0.75f, 20.0f);
            const Math::Vec3 end = position + direction * length;

            debugDraw.DrawLine(position, end, DirectionalLightIndicatorColor);

            Math::Vec3 right = Math::Cross(direction, Math::Vec3(0.0f, 1.0f, 0.0f));
            right = Math::Length(right) > 0.001f ? Math::Normalize(right) : Math::Vec3(1.0f, 0.0f, 0.0f);
            const Math::Vec3 up = Math::Normalize(Math::Cross(right, direction));

            const float headLength = length * 0.22f;
            const float headWidth = length * 0.09f;
            const Math::Vec3 headBase = end - direction * headLength;

            debugDraw.DrawLine(end, headBase + right * headWidth, DirectionalLightIndicatorColor);
            debugDraw.DrawLine(end, headBase - right * headWidth, DirectionalLightIndicatorColor);
            debugDraw.DrawLine(end, headBase + up * headWidth, DirectionalLightIndicatorColor);
            debugDraw.DrawLine(end, headBase - up * headWidth, DirectionalLightIndicatorColor);
        }
    }

    void DrawSelectionIndicators(EditorContext& context, DebugDraw& debugDraw)
    {
        if (context.ActiveScene == nullptr || !context.SelectedEntity)
        {
            return;
        }

        const Entity selected = context.SelectedEntity;
        if (!selected.HasComponent<TransformComponent>())
        {
            return;
        }

        if (!selected.HasComponent<CameraComponent>() && !selected.HasComponent<LightComponent>())
        {
            return;
        }

        const Math::Mat4 worldTransform = context.ActiveScene->GetWorldTransform(selected.GetHandle());
        debugDraw.Begin(context.Camera.GetViewProjection());

        if (selected.HasComponent<CameraComponent>())
        {
            DrawCameraIndicator(debugDraw, context, worldTransform);
        }

        if (selected.HasComponent<LightComponent>())
        {
            const auto& light = selected.GetComponent<LightComponent>();
            if (light.Type == LightComponent::LightType::Point)
            {
                DrawPointLightIndicator(debugDraw, light, worldTransform);
            }
            else
            {
                DrawDirectionalLightIndicator(debugDraw, context, worldTransform);
            }
        }

        debugDraw.End();
    }
}
