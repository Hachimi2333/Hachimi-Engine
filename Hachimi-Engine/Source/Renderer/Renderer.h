#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/RendererAPI.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    class VertexArray;

    // Static facade around the active renderer backend.
    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        static void SetClearColor(const Math::Vec4& color);
        static void Clear();
        static void SetDepthTest(bool enabled);
        static void SetPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f);
        static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, DrawMode drawMode = DrawMode::Triangles);

        static RendererAPIType GetAPI();

    private:
        static Scope<RendererAPI> s_RendererAPI;
    };
}
