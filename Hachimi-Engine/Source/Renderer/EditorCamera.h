#pragma once

#include "Core/Base.h"
#include "Core/Timestep.h"
#include "Renderer/Camera.h"

#include <glm/glm.hpp>

namespace HachimiEngine
{
    // Orbit/pan editor camera driven by the viewport panel.
    class EditorCamera final : public Camera
    {
    public:
        EditorCamera();

        void OnUpdate(Timestep timestep);
        void OnMouseScroll(float yOffset);
        void OnMouseDrag(const glm::vec2& mouseDelta, int mouseButton);
        void SetViewportSize(uint32_t width, uint32_t height);

        const glm::mat4& GetViewMatrix() const;
        const glm::mat4& GetViewProjection() const;

        glm::vec3 GetPosition() const { return m_Position; }
        glm::vec3 GetFocalPoint() const { return m_FocalPoint; }
        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        float GetDistance() const { return m_Distance; }

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        float GetFieldOfView() const { return m_FieldOfView; }

        void SetPosition(const glm::vec3& position) { m_Position = position; }
        void SetFocalPoint(const glm::vec3& focalPoint) { m_FocalPoint = focalPoint; }
        void SetViewportSize(float width, float height);
        void SetFieldOfView(float fieldOfView);

        void Move(const glm::vec3& delta);
        void SetDistance(float distance);

    private:
        void RecalculateView();
        void RecalculateProjection();

        glm::mat4 m_ViewMatrix { 1.0f };
        mutable glm::mat4 m_ViewProjectionCache { 1.0f };
        mutable bool m_ViewProjectionDirty = true;

        glm::vec3 m_Position { 0.0f, 3.0f, 8.0f };
        glm::vec3 m_FocalPoint { 0.0f, 1.0f, 0.0f };
        glm::vec3 m_WorldUp { 0.0f, 1.0f, 0.0f };

        float m_FieldOfView = 45.0f;
        float m_AspectRatio = 1.778f;
        float m_NearClip = 0.1f;
        float m_FarClip = 1000.0f;

        float m_Distance = 8.0f;
        float m_Pitch = -0.35f;
        float m_Yaw = -1.5708f;

        float m_MoveSpeed = 8.0f;
        float m_MouseSensitivity = 0.008f;
        float m_ZoomSpeed = 0.25f;
    };
}
