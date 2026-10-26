#include "Renderer/DebugDraw.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Renderer/Renderer.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace HachimiEngine
{
    namespace
    {
        // One frame's worth of debug lines in a single buffer. The editor gizmos use a few dozen of
        // them; the physics view draws every collider in the scene, which is what sets the size - a
        // box is twelve lines, a sphere ninety-six, so this holds a few thousand colliders.
        constexpr uint32_t MaxVertexCount = 32768;

        void DrawCircle(
            DebugDraw& debugDraw,
            const Math::Vec3& center,
            const Math::Vec3& axisA,
            const Math::Vec3& axisB,
            float radius,
            const Math::Vec4& color,
            uint32_t segments)
        {
            segments = std::max(segments, 4u);

            Math::Vec3 previous = center + axisA * radius;
            for (uint32_t index = 1; index <= segments; ++index)
            {
                const float angle = Math::TwoPi<float>() * static_cast<float>(index) / static_cast<float>(segments);
                const Math::Vec3 current = center
                    + (axisA * std::cos(angle) + axisB * std::sin(angle)) * radius;
                debugDraw.DrawLine(previous, current, color);
                previous = current;
            }
        }
    }

    DebugDraw::DebugDraw()
    {
        m_Shader = Shader::CreateEngineShader("DebugDraw.glsl");

        m_VertexBuffer = VertexBuffer::Create(MaxVertexCount * static_cast<uint32_t>(sizeof(Vertex)));
        m_VertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" }
        });

        std::vector<uint32_t> indices(MaxVertexCount);
        std::iota(indices.begin(), indices.end(), 0u);

        m_IndexBuffer = IndexBuffer::Create(indices.data(), MaxVertexCount);

        m_VertexArray = VertexArray::Create();
        m_VertexArray->AddVertexBuffer(m_VertexBuffer);
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);

        m_Vertices.reserve(MaxVertexCount);
    }

    DebugDraw::~DebugDraw()
    {
        m_Vertices.clear();
        m_ViewProjection = Math::Mat4(1.0f);
        m_VertexArray.reset();
        m_IndexBuffer.reset();
        m_VertexBuffer.reset();
        m_Shader.reset();
    }

    void DebugDraw::Begin(const Math::Mat4& viewProjection)
    {
        m_ViewProjection = viewProjection;
        m_Vertices.clear();
        m_OverflowReported = false;
    }

    void DebugDraw::DrawLine(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color)
    {
        // Everything submitted between Begin and End is uploaded as one buffer, so a frame that asks
        // for more lines than it holds gets the ones that fit and a single report. Dropping is the
        // lesser evil here: an overflowing upload would be a corrupted draw at best.
        if (m_Vertices.size() + 2 > MaxVertexCount)
        {
            if (!m_OverflowReported)
            {
                m_OverflowReported = true;
                HE_CORE_WARN(
                    "Debug draw holds {} vertices per frame; the rest of this frame's lines were dropped",
                    MaxVertexCount);
            }
            return;
        }

        m_Vertices.push_back({ start, color });
        m_Vertices.push_back({ end, color });
    }

    void DebugDraw::DrawSphere(const Math::Vec3& center, float radius, const Math::Vec4& color, uint32_t segments)
    {
        radius = std::max(radius, 0.001f);

        DrawCircle(*this, center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, radius, color, segments);
        DrawCircle(*this, center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color, segments);
        DrawCircle(*this, center, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color, segments);
    }

    void DebugDraw::DrawBox(
        const Math::Vec3& halfExtents,
        const Math::Vec3& center,
        const Math::Quat& rotation,
        const Math::Vec4& color)
    {
        const Math::Vec3 extents(
            std::abs(halfExtents.x),
            std::abs(halfExtents.y),
            std::abs(halfExtents.z));

        Math::Vec3 corners[8];
        for (int index = 0; index < 8; ++index)
        {
            const Math::Vec3 offset(
                (index & 1) != 0 ? extents.x : -extents.x,
                (index & 2) != 0 ? extents.y : -extents.y,
                (index & 4) != 0 ? extents.z : -extents.z);
            corners[index] = center + Math::Rotate(rotation, offset);
        }

        // Corners are indexed by sign bits, so two corners share an edge when their indices differ in
        // exactly one bit: three bits, four edges each, twelve edges.
        for (int index = 0; index < 8; ++index)
        {
            for (int bit = 1; bit <= 4; bit <<= 1)
            {
                if ((index & bit) == 0)
                {
                    DrawLine(corners[index], corners[index | bit], color);
                }
            }
        }
    }

    void DebugDraw::DrawCapsule(
        const Math::Vec3& start,
        const Math::Vec3& end,
        float radius,
        const Math::Vec4& color,
        uint32_t segments)
    {
        radius = std::max(radius, 0.001f);

        Math::Vec3 axis = end - start;
        const float length = Math::Length(axis);
        if (length <= 1e-5f)
        {
            // A degenerate capsule is a sphere, which is also the honest drawing for one.
            DrawSphere(start, radius, color, segments);
            return;
        }

        axis /= length;

        // Any two directions perpendicular to the axis will do; picking the smallest component to
        // cross against keeps the result well conditioned for every axis direction.
        const Math::Vec3 reference = std::abs(axis.y) < 0.99f ? Math::Vec3(0.0f, 1.0f, 0.0f) : Math::Vec3(1.0f, 0.0f, 0.0f);
        const Math::Vec3 sideA = Math::Normalize(Math::Cross(axis, reference));
        const Math::Vec3 sideB = Math::Cross(axis, sideA);

        DrawCircle(*this, start, sideA, sideB, radius, color, segments);
        DrawCircle(*this, end, sideA, sideB, radius, color, segments);

        for (const Math::Vec3& side : { sideA, sideB })
        {
            DrawLine(start + side * radius, end + side * radius, color);
            DrawLine(start - side * radius, end - side * radius, color);
        }
    }

    void DebugDraw::DrawPoint(const Math::Vec3& position, float extent, const Math::Vec4& color)
    {
        extent = std::max(extent, 0.001f);

        DrawLine(position - Math::Vec3(extent, 0.0f, 0.0f), position + Math::Vec3(extent, 0.0f, 0.0f), color);
        DrawLine(position - Math::Vec3(0.0f, extent, 0.0f), position + Math::Vec3(0.0f, extent, 0.0f), color);
        DrawLine(position - Math::Vec3(0.0f, 0.0f, extent), position + Math::Vec3(0.0f, 0.0f, extent), color);
    }

    void DebugDraw::DrawAxes(const Math::Vec3& origin, float size)
    {
        size = std::max(size, 0.001f);

        DrawLine(origin, origin + Math::Vec3(size, 0.0f, 0.0f), { 1.0f, 0.2f, 0.2f, 1.0f });
        DrawLine(origin, origin + Math::Vec3(0.0f, size, 0.0f), { 0.2f, 1.0f, 0.2f, 1.0f });
        DrawLine(origin, origin + Math::Vec3(0.0f, 0.0f, size), { 0.25f, 0.45f, 1.0f, 1.0f });
    }

    void DebugDraw::End()
    {
        if (m_Shader == nullptr || m_VertexArray == nullptr || m_VertexBuffer == nullptr || m_Vertices.empty())
        {
            return;
        }

        const uint32_t dataSize = static_cast<uint32_t>(m_Vertices.size() * sizeof(Vertex));
        m_VertexBuffer->SetData(m_Vertices.data(), dataSize);

        m_Shader->Bind();
        m_Shader->SetMat4("u_ViewProjection", m_ViewProjection);

        Renderer::SetDepthTest(true);
        glDepthMask(GL_FALSE);
        Renderer::DrawIndexed(m_VertexArray, static_cast<uint32_t>(m_Vertices.size()), DrawMode::Lines);
        glDepthMask(GL_TRUE);
    }
}
