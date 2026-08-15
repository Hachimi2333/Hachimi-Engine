#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Buffer.h"
#include "Renderer/VertexArray.h"

#include <glm/glm.hpp>

#include <vector>

namespace HachimiEngine
{
    enum class MeshDrawMode
    {
        Triangles = 0,
        Lines = 1
    };

    struct MeshVertex
    {
        glm::vec3 Position { 0.0f };
        glm::vec3 Normal { 0.0f, 1.0f, 0.0f };
        glm::vec2 TexCoord { 0.0f };
        glm::vec4 Color { 1.0f };

        static BufferLayout GetLayout();
    };

    // CPU/GPU mesh pair built from vertex and index data.
    class Mesh
    {
    public:
        Mesh(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode = MeshDrawMode::Triangles);
        ~Mesh() = default;

        void Bind() const;
        void Unbind() const;

        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
        MeshDrawMode GetDrawMode() const { return m_DrawMode; }
        uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

        static Ref<Mesh> Create(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode = MeshDrawMode::Triangles);

    private:
        void Build();

    private:
        std::vector<MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        MeshDrawMode m_DrawMode = MeshDrawMode::Triangles;
        Ref<VertexArray> m_VertexArray;
    };
}
