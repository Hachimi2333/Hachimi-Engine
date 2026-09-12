#include "Renderer/Mesh.h"

#include "Core/Assert.h"

namespace HachimiEngine
{
    Mesh::Mesh(const Ref<MeshData>& meshData)
        : m_MeshData(meshData)
    {
        BuildVertexArray();
    }

    void Mesh::BuildVertexArray()
    {
        HE_CORE_ASSERT(m_MeshData != nullptr);
        HE_CORE_ASSERT(!m_MeshData->IsEmpty());

        m_VertexArray = VertexArray::Create();

        const std::vector<MeshVertex>& vertices = m_MeshData->GetVertices();
        const std::vector<uint32_t>& indices = m_MeshData->GetIndices();

        const uint32_t vertexDataSize = static_cast<uint32_t>(vertices.size() * sizeof(MeshVertex));
        const Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            reinterpret_cast<const float*>(vertices.data()),
            vertexDataSize);
        vertexBuffer->SetLayout(MeshVertex::GetLayout());
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        const Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        m_VertexArray->SetIndexBuffer(indexBuffer);
    }

    Ref<Mesh> Mesh::Create(const Ref<MeshData>& meshData)
    {
        if (meshData == nullptr || meshData->IsEmpty())
        {
            return nullptr;
        }

        // The constructor is private, so CreateRef cannot be used here.
        return Ref<Mesh>(new Mesh(meshData));
    }
}
