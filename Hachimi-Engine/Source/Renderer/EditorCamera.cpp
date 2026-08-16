#include "Renderer/EditorCamera.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
#include "Math/Math.h"

#include <algorithm>
#include <cmath>

namespace HachimiEngine
{
    namespace
    {
        // Control constants for the editor camera's orbit/pan/zoom feel.
        constexpr float OrbitSensitivity = 0.006f;
        constexpr float PanScalePerUnit = 0.0015f;
        constexpr float MinimumPanScale = 0.002f;
        constexpr float WheelZoomRate = 0.14f;
        constexpr float AltDragZoomRate = 0.01f;
        constexpr float MinimumDistance = 0.15f;
        constexpr float MaximumDistance = 5000.0f;
        constexpr float NormalMoveSpeed = 4.0f;
        constexpr float FastMoveSpeed = 12.0f;
        constexpr float MoveSpeedDistanceFactor = 0.15f;
        constexpr float HalfPi = 1.57079632679f;
        constexpr float MaximumPitch = HalfPi - 0.02f;
    }

    EditorCamera::EditorCamera()
    {
        RecalculateProjection();
        RecalculateView();
    }

    void EditorCamera::OnUpdate(Timestep timestep, bool viewportHovered)
    {
        // WASD/QE flight only happens while the right mouse button is held and
        // the cursor is over the viewport.
        if (!viewportHovered || !Input::IsMouseButtonPressed(Mouse::ButtonRight))
        {
            return;
        }

        const float speed = (Input::IsKeyPressed(Key::LeftShift) ? FastMoveSpeed : NormalMoveSpeed)
            * std::max(1.0f, m_Distance * MoveSpeedDistanceFactor);

        Math::Vec3 movement(0.0f);

        if (Input::IsKeyPressed(Key::W))
        {
            movement += GetForwardDirection();
        }
        if (Input::IsKeyPressed(Key::S))
        {
            movement -= GetForwardDirection();
        }
        if (Input::IsKeyPressed(Key::D))
        {
            movement += GetRightDirection();
        }
        if (Input::IsKeyPressed(Key::A))
        {
            movement -= GetRightDirection();
        }
        if (Input::IsKeyPressed(Key::E))
        {
            movement += m_WorldUp;
        }
        if (Input::IsKeyPressed(Key::Q))
        {
            movement -= m_WorldUp;
        }

        if (Math::Length(movement) > 0.0f)
        {
            const float delta = speed * timestep.GetSeconds();
            m_FocalPoint += Math::Normalize(movement) * delta;
            RecalculateView();
        }
    }

    void EditorCamera::OnMouseScroll(float yOffset)
    {
        m_Distance = std::clamp(m_Distance * std::exp(-yOffset * WheelZoomRate), MinimumDistance, MaximumDistance);
        RecalculateView();
    }

    void EditorCamera::OnMouseDrag(const Math::Vec2& mouseDelta, int mouseButton)
    {
        const bool altPressed = Input::IsKeyPressed(Key::LeftAlt);

        if (mouseButton == Mouse::ButtonRight && altPressed)
        {
            m_Distance = std::clamp(
                m_Distance * std::exp(mouseDelta.y * AltDragZoomRate),
                MinimumDistance,
                MaximumDistance);
        }
        else if (mouseButton == Mouse::ButtonRight || (mouseButton == Mouse::ButtonLeft && altPressed))
        {
            m_Yaw -= mouseDelta.x * OrbitSensitivity;
            m_Pitch = std::clamp(m_Pitch + mouseDelta.y * OrbitSensitivity, -MaximumPitch, MaximumPitch);
        }
        else if (mouseButton == Mouse::ButtonMiddle)
        {
            const Math::Vec3 right = GetRightDirection();
            const Math::Vec3 up = GetUpDirection();
            const float panScale = std::max(MinimumPanScale, m_Distance * PanScalePerUnit);

            m_FocalPoint += -right * mouseDelta.x * panScale;
            m_FocalPoint += up * mouseDelta.y * panScale;
        }

        RecalculateView();
    }

    void EditorCamera::SetViewportSize(uint32_t width, uint32_t height)
    {
        SetViewportSize(static_cast<float>(width), static_cast<float>(height));
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        if (height <= 0.0f)
        {
            height = 1.0f;
        }
        if (width <= 0.0f)
        {
            width = 1.0f;
        }

        m_AspectRatio = width / height;
        RecalculateProjection();
    }

    void EditorCamera::SetFieldOfView(float fieldOfView)
    {
        m_FieldOfView = fieldOfView;
        RecalculateProjection();
    }

    void EditorCamera::Move(const Math::Vec3& delta)
    {
        m_Position += delta;
        m_FocalPoint += delta;
        RecalculateView();
    }

    void EditorCamera::SetDistance(float distance)
    {
        m_Distance = std::clamp(distance, MinimumDistance, MaximumDistance);
        RecalculateView();
    }

    const Math::Mat4& EditorCamera::GetViewMatrix() const
    {
        return m_ViewMatrix;
    }

    const Math::Mat4& EditorCamera::GetViewProjection() const
    {
        if (m_ViewProjectionDirty)
        {
            m_ViewProjectionCache = m_Projection * m_ViewMatrix;
            m_ViewProjectionDirty = false;
        }
        return m_ViewProjectionCache;
    }

    Math::Vec3 EditorCamera::GetUpDirection() const
    {
        const Math::Vec3 right = GetRightDirection();
        return Math::Normalize(Math::Cross(right, GetForwardDirection()));
    }

    Math::Vec3 EditorCamera::GetRightDirection() const
    {
        return Math::Normalize(Math::Cross(GetForwardDirection(), m_WorldUp));
    }

    Math::Vec3 EditorCamera::GetForwardDirection() const
    {
        const float cosPitch = std::cos(m_Pitch);
        return Math::Normalize(Math::Vec3(
            std::sin(m_Yaw) * cosPitch,
            std::sin(m_Pitch),
            std::cos(m_Yaw) * cosPitch));
    }

    void EditorCamera::RecalculateView()
    {
        m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
        m_ViewMatrix = Math::LookAt(m_Position, m_FocalPoint, m_WorldUp);
        RecalculateProjection();
        m_ViewProjectionDirty = true;
    }

    void EditorCamera::RecalculateProjection()
    {
        // Dynamic clip planes scale with distance so both small scenes and very
        // large imported scenes stay visible, matching the previous editor camera.
        const float nearClip = std::clamp(m_Distance * 0.001f, 0.01f, 1.0f);
        const float farClip = std::max(2000.0f, m_Distance * 50.0f);

        m_Projection = Math::Perspective(Math::Radians(m_FieldOfView), m_AspectRatio, nearClip, farClip);
        m_ViewProjectionDirty = true;
    }
}
