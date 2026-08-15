#pragma once

#include "Renderer/RendererAPI.h"
#include "Renderer/VertexArray.h"

namespace HachimiEngine
{
    class OpenGLRendererAPI final : public RendererAPI
    {
    public:
        void Init() override;

        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        void SetClearColor(const glm::vec4& color) override;
        void Clear() override;

        void SetDepthTest(bool enabled) override;
        void SetBlend(bool enabled) override;
        void SetLineWidth(float width) override;

        void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
    };
}
