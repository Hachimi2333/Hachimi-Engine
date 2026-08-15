#include "Renderer/EditorCamera.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>

namespace HachimiEngine
{
    EditorCamera::EditorCamera()
    {
        RecalculateProjection();
        RecalculateView();
    }

    void EditorCamera::OnUpdate(Timestep timestep)
    {
        const float speed = m_MoveSpeed * (Input::IsKeyPressed(Key::LeftShift) ? 2.0f : 1.0f);
        glm::vec3 movement(0.0f);

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
            movement += GetUpDirection();
        }
        if (Input::IsKeyPressed(Key::Q))
        {
            movement -= GetUpDirection();
        }

        if (glm::length(movement) > 0.0f)
        {
            const float delta = speed * timestep.GetSeconds();
            m_Position += glm::normalize(movement) * delta;
            m_FocalPoint += glm::normalize(movement) * delta;
            RecalculateView();
        }
    }

    void EditorCamera::OnMouseScroll(float yOffset)
    {
        const float delta = -yOffset * m_ZoomSpeed * std::max(m_Distance * 0.1f, 0.1f);
        SetDistance(m_Distance + delta);
    }

    void EditorCamera::OnMouseDrag(const glm::vec2& mouseDelta, int mouseButton)
    {
        if (mouseButton == Mouse::ButtonRight)
        {
            m_Yaw -= mouseDelta.x * m_MouseSensitivity;
            m_Pitch -= mouseDelta.y * m_MouseSensitivity;
        }
        else if (mouseButton == Mouse::ButtonMiddle)
        {
            const glm::vec3 right = GetRightDirection();
            const glm::vec3 up = GetUpDirection();
            const float panScale = m_Distance * 0.0025f;

            m_FocalPoint += -right * mouseDelta.x * panScale;
            m_FocalPoint += up * mouseDelta.y * panScale;
        }
        else if (mouseButton == Mouse::ButtonLeft && Input::IsKeyPressed(Key::LeftAlt))
        {
            m_Yaw -= mouseDelta.x * m_MouseSensitivity;
            m_Pitch -= mouseDelta.y * m_MouseSensitivity;
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

    void EditorCamera::Move(const glm::vec3& delta)
    {
        m_Position += delta;
        m_FocalPoint += delta;
        RecalculateView();
    }

    void EditorCamera::SetDistance(float distance)
    {
        m_Distance = std::clamp(distance, 0.1f, 200.0f);
        m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
        RecalculateView();
    }

    const glm::mat4& EditorCamera::GetViewMatrix() const
    {
        return m_ViewMatrix;
    }

    const glm::mat4& EditorCamera::GetViewProjection() const
    {
        if (m_ViewProjectionDirty)
        {
            m_ViewProjectionCache = m_Projection * m_ViewMatrix;
            m_ViewProjectionDirty = false;
        }
        return m_ViewProjectionCache;
    }

    glm::vec3 EditorCamera::GetUpDirection() const
    {
        return glm::rotate(glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f)), m_WorldUp);
    }

    glm::vec3 EditorCamera::GetRightDirection() const
    {
        return glm::rotate(glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 EditorCamera::GetForwardDirection() const
    {
        return glm::rotate(glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f)), glm::vec3(0.0f, 0.0f, -1.0f));
    }

    void EditorCamera::RecalculateView()
    {
        m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
        m_ViewMatrix = glm::lookAt(m_Position, m_FocalPoint, m_WorldUp);
        m_ViewProjectionDirty = true;
    }

    void EditorCamera::RecalculateProjection()
    {
        m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearClip, m_FarClip);
        m_ViewProjectionDirty = true;
    }
}
