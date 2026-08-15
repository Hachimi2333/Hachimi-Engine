#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <glm/glm.hpp>

#include <string>

namespace HachimiEngine
{
    // Rendering backends. Only OpenGL 4.6 Core is implemented in the current phase.
    enum class RendererAPIType
    {
        None = 0,
        OpenGL = 1
    };

    // Draw primitive topology used by the simple backend abstraction.
    enum class DrawMode
    {
        Triangles = 0,
        Lines = 1
    };

    // Small OpenGL-style backend abstraction; intentionally simple rather than Vulkan-like.
    class RendererAPI
    {
    public:
        virtual ~RendererAPI() = default;

        virtual void Init() = 0;

        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;

        virtual void SetDepthTest(bool enabled) = 0;
        virtual void SetBlend(bool enabled) = 0;
        virtual void SetLineWidth(float width) = 0;
        virtual void SetPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) = 0;

        virtual void DrawIndexed(const Ref<class VertexArray>& vertexArray, uint32_t indexCount = 0, DrawMode drawMode = DrawMode::Triangles) = 0;

        static RendererAPIType GetAPI();
        static Scope<RendererAPI> Create();
    };
}
