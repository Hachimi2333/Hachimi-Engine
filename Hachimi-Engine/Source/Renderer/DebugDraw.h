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
        // Oriented box as twelve edges, which is what a collider of any shape reduces to once the
        // renderer is line based.
        void DrawBox(const Math::Vec3& halfExtents, const Math::Vec3& center, const Math::Quat& rotation,
                     const Math::Vec4& color);
        // Capsule as the two end caps plus the four connecting edges. The caps are squashed rings
        // rather than hemispheres: an editor reads the silhouette and the radius from them, and a
        // hemisphere is a lot of lines for the same information.
        void DrawCapsule(const Math::Vec3& start, const Math::Vec3& end, float radius, const Math::Vec4& color,
                         uint32_t segments = 16);
        // A point as a small three-axis cross, since lines are all this renderer draws.
        void DrawPoint(const Math::Vec3& position, float extent, const Math::Vec4& color);
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
        // Reported once per frame, so a scene larger than the line buffer says so instead of either
        // flooding the console or silently drawing half of itself.
        bool m_OverflowReported = false;
    };
}
