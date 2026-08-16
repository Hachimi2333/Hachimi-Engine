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
        constexpr const char* VertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

        constexpr const char* FragmentShader = R"(
#version 460 core

layout(location = 0) out vec4 o_Color;

in vec4 v_Color;

void main()
{
    o_Color = v_Color;
}
)";

        // 4096 lines (8192 vertices) is far more than the editor gizmos submit.
        constexpr uint32_t MaxVertexCount = 8192;

        void DrawCircle(
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
                DebugDraw::DrawLine(previous, current, color);
                previous = current;
            }
        }
    }

    Ref<Shader> DebugDraw::s_Shader;
    Ref<VertexBuffer> DebugDraw::s_VertexBuffer;
    Ref<IndexBuffer> DebugDraw::s_IndexBuffer;
    Ref<VertexArray> DebugDraw::s_VertexArray;
    std::vector<DebugDraw::Vertex> DebugDraw::s_Vertices;
    Math::Mat4 DebugDraw::s_ViewProjection { 1.0f };

    void DebugDraw::Init()
    {
        s_Shader = Shader::Create("DebugDraw", VertexShader, FragmentShader);

        s_VertexBuffer = VertexBuffer::Create(MaxVertexCount * static_cast<uint32_t>(sizeof(Vertex)));
        s_VertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" }
        });

        std::vector<uint32_t> indices(MaxVertexCount);
        std::iota(indices.begin(), indices.end(), 0u);

        s_IndexBuffer = IndexBuffer::Create(indices.data(), MaxVertexCount);

        s_VertexArray = VertexArray::Create();
        s_VertexArray->AddVertexBuffer(s_VertexBuffer);
        s_VertexArray->SetIndexBuffer(s_IndexBuffer);

        s_Vertices.reserve(MaxVertexCount);
    }

    void DebugDraw::Shutdown()
    {
        s_Vertices.clear();
        s_ViewProjection = Math::Mat4(1.0f);
        s_VertexArray.reset();
        s_IndexBuffer.reset();
        s_VertexBuffer.reset();
        s_Shader.reset();
    }

    void DebugDraw::Begin(const Math::Mat4& viewProjection)
    {
        s_ViewProjection = viewProjection;
        s_Vertices.clear();
    }

    void DebugDraw::DrawLine(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color)
    {
        HE_CORE_ASSERT(s_Vertices.size() + 2 <= MaxVertexCount);
        s_Vertices.push_back({ start, color });
        s_Vertices.push_back({ end, color });
    }

    void DebugDraw::DrawSphere(const Math::Vec3& center, float radius, const Math::Vec4& color, uint32_t segments)
    {
        radius = std::max(radius, 0.001f);

        DrawCircle(center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, radius, color, segments);
        DrawCircle(center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color, segments);
        DrawCircle(center, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color, segments);
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
        if (s_Shader == nullptr || s_VertexArray == nullptr || s_VertexBuffer == nullptr || s_Vertices.empty())
        {
            return;
        }

        const uint32_t dataSize = static_cast<uint32_t>(s_Vertices.size() * sizeof(Vertex));
        s_VertexBuffer->SetData(s_Vertices.data(), dataSize);

        s_Shader->Bind();
        s_Shader->SetMat4("u_ViewProjection", s_ViewProjection);

        Renderer::SetDepthTest(true);
        glDepthMask(GL_FALSE);
        Renderer::DrawIndexed(s_VertexArray, static_cast<uint32_t>(s_Vertices.size()), DrawMode::Lines);
        glDepthMask(GL_TRUE);
    }
}
