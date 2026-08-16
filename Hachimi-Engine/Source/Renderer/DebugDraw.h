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
    // Call Begin once per viewport, submit lines, then call End to upload and draw.
    class DebugDraw
    {
    public:
        static void Init();
        static void Shutdown();

        static void Begin(const Math::Mat4& viewProjection);
        static void DrawLine(const Math::Vec3& start, const Math::Vec3& end, const Math::Vec4& color);
        static void DrawSphere(const Math::Vec3& center, float radius, const Math::Vec4& color, uint32_t segments = 32);
        static void DrawAxes(const Math::Vec3& origin, float size);
        static void End();

    private:
        struct Vertex
        {
            Math::Vec3 Position { 0.0f };
            Math::Vec4 Color { 1.0f };
        };

        static Ref<Shader> s_Shader;
        static Ref<VertexBuffer> s_VertexBuffer;
        static Ref<IndexBuffer> s_IndexBuffer;
        static Ref<VertexArray> s_VertexArray;
        static std::vector<Vertex> s_Vertices;
        static Math::Mat4 s_ViewProjection;
    };
}
