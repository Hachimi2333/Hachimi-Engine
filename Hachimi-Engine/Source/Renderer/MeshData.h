#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Buffer.h"
#include "Math/Math.h"

#include <cstdint>
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
        Math::Vec3 Position { 0.0f };
        Math::Vec3 Normal { 0.0f, 1.0f, 0.0f };
        Math::Vec2 TexCoord { 0.0f };
        Math::Vec4 Color { 1.0f };

        static BufferLayout GetLayout();
    };

    // CPU-side mesh geometry and nothing else: no vertex array, no OpenGL handle.
    //
    // Keeping geometry on this side of the seam is what lets a Scene, the scene
    // serializer, the physics world and the script world build, clone and inspect
    // meshes without an OpenGL context. MeshLibrary turns one of these into a GPU
    // Mesh on demand, and MeshFactory produces them for the built-in primitives.
    class MeshData
    {
    public:
        MeshData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices,
                 MeshDrawMode drawMode = MeshDrawMode::Triangles);

        const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

        MeshDrawMode GetDrawMode() const { return m_DrawMode; }
        uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
        uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

        // Local-space bounds, computed once from the vertices.
        const Math::AABB& GetBounds() const { return m_Bounds; }

        // True when there is nothing to draw, which is also when no GPU mesh is built.
        bool IsEmpty() const { return m_Vertices.empty() || m_Indices.empty(); }

        static Ref<MeshData> Create(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices,
                                    MeshDrawMode drawMode = MeshDrawMode::Triangles);

    private:
        void RecalculateBounds();

    private:
        std::vector<MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        MeshDrawMode m_DrawMode = MeshDrawMode::Triangles;
        Math::AABB m_Bounds;
    };
}
