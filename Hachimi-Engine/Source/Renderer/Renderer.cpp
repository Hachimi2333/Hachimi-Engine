#include "Renderer/Renderer.h"

#include "Renderer/RendererAPI.h"
#include "Renderer/VertexArray.h"

namespace HachimiEngine
{
    Scope<RendererAPI> Renderer::s_RendererAPI;

    void Renderer::Init()
    {
        s_RendererAPI = RendererAPI::Create();
        s_RendererAPI->Init();
    }

    void Renderer::Shutdown()
    {
        s_RendererAPI.reset();
    }

    void Renderer::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        s_RendererAPI->SetViewport(x, y, width, height);
    }

    void Renderer::SetClearColor(const glm::vec4& color)
    {
        s_RendererAPI->SetClearColor(color);
    }

    void Renderer::Clear()
    {
        s_RendererAPI->Clear();
    }

    void Renderer::SetDepthTest(bool enabled)
    {
        s_RendererAPI->SetDepthTest(enabled);
    }

    void Renderer::SetPolygonOffset(bool enabled, float factor, float units)
    {
        s_RendererAPI->SetPolygonOffset(enabled, factor, units);
    }

    void Renderer::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, DrawMode drawMode)
    {
        s_RendererAPI->DrawIndexed(vertexArray, indexCount, drawMode);
    }

    RendererAPIType Renderer::GetAPI()
    {
        return RendererAPI::GetAPI();
    }
}
