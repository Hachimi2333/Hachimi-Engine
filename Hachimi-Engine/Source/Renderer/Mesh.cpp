#include "Renderer/Mesh.h"

namespace HachimiEngine
{
    BufferLayout MeshVertex::GetLayout()
    {
        return {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float4, "a_Color" }
        };
    }

    Mesh::Mesh(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode)
        : m_Vertices(std::move(vertices)), m_Indices(std::move(indices)), m_DrawMode(drawMode)
    {
        Build();
    }

    void Mesh::Bind() const
    {
        m_VertexArray->Bind();
    }

    void Mesh::Unbind() const
    {
        m_VertexArray->Unbind();
    }

    void Mesh::Build()
    {
        m_VertexArray = VertexArray::Create();

        const uint32_t vertexDataSize = static_cast<uint32_t>(m_Vertices.size() * sizeof(MeshVertex));
        const Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            reinterpret_cast<const float*>(m_Vertices.data()),
            vertexDataSize);
        vertexBuffer->SetLayout(MeshVertex::GetLayout());
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        const Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
        m_VertexArray->SetIndexBuffer(indexBuffer);
    }

    Ref<Mesh> Mesh::Create(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode)
    {
        return CreateRef<Mesh>(std::move(vertices), std::move(indices), drawMode);
    }
}
