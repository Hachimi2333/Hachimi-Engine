#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Buffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Math/Math.h"

#include <vector>

namespace HachimiEngine
{
    // Immediate-mode GPU line renderer used for editor debug visuals.
    // Call Begin once per view, submit lines, then call End to upload and draw.
    class DebugDraw
    {
    public:
        // Creates the line shader and the reusable buffers; needs a current GL context.
        DebugDraw();
        ~DebugDraw();

        DebugDraw(const DebugDraw&) = delete;
        DebugDraw& operator=(const DebugDraw&) = delete;

        void Begin(const Math::Mat4& viewProjection);
        void DrawLine(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color);
        void DrawSphere(const Math::Vec3& center, float radius, const Math::Vec4& color, uint32_t segments = 32);
        void DrawAxes(const Math::Vec3& origin, float size);
        void End();

    private:
        struct Vertex
        {
            Math::Vec3 Position { 0.0f };
            Math::Vec4 Color { 1.0f };
        };

        Ref<Shader> m_Shader;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<VertexArray> m_VertexArray;
        std::vector<Vertex> m_Vertices;
        Math::Mat4 m_ViewProjection { 1.0f };
    };
}
