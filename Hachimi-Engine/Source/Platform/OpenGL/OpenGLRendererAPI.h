#pragma once

#include "Renderer/RendererAPI.h"
#include "Renderer/VertexArray.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    class OpenGLRendererAPI final : public RendererAPI
    {
    public:
        void Init() override;

        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        void SetClearColor(const Math::Vec4& color) override;
        void Clear() override;

        void SetDepthTest(bool enabled) override;
        void SetBlend(bool enabled) override;
        void SetLineWidth(float width) override;
        void SetPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) override;

        void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, DrawMode drawMode = DrawMode::Triangles) override;
    };
}
