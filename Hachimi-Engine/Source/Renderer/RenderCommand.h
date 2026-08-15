#pragma once

#include "Renderer/Renderer.h"

namespace HachimiEngine
{
    // Thin command layer so client code never talks to a concrete backend directly.
    class RenderCommand
    {
    public:
        static void Init() { Renderer::Init(); }
        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) { Renderer::SetViewport(x, y, width, height); }
        static void SetClearColor(const glm::vec4& color) { Renderer::SetClearColor(color); }
        static void Clear() { Renderer::Clear(); }
        static void SetDepthTest(bool enabled) { Renderer::SetDepthTest(enabled); }
        static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, DrawMode drawMode = DrawMode::Triangles) { Renderer::DrawIndexed(vertexArray, indexCount, drawMode); }
    };
}
