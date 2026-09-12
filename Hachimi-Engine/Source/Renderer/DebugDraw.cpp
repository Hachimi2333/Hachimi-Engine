#include "Renderer/DebugDraw.h"

#include "Core/Assert.h"
#include "Renderer/Renderer.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace HachimiEngine
{
    namespace
    {
        // 4096 lines (8192 vertices) is far more than the editor gizmos submit.
        constexpr uint32_t MaxVertexCount = 8192;

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
    }

    void DebugDraw::DrawLine(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color)
    {
        HE_CORE_ASSERT(m_Vertices.size() + 2 <= MaxVertexCount);
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
