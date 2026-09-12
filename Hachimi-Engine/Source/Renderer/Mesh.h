#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/MeshData.h"
#include "Renderer/VertexArray.h"
#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    // GPU counterpart of MeshData: the vertex array uploaded for one CPU mesh.
    //
    // Constructing a Mesh issues OpenGL calls, so a Mesh only ever exists while a
    // context is current. MeshLibrary owns them and guarantees one upload per
    // MeshData, which is why scene code never calls Mesh::Create directly.
    class Mesh
    {
    public:
        ~Mesh() = default;

        const Ref<MeshData>& GetMeshData() const { return m_MeshData; }
        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }

        MeshDrawMode GetDrawMode() const { return m_MeshData->GetDrawMode(); }
        uint32_t GetIndexCount() const { return m_MeshData->GetIndexCount(); }
        const Math::AABB& GetBounds() const { return m_MeshData->GetBounds(); }

        // Returns nullptr for null or empty geometry, so callers can skip the draw.
        static Ref<Mesh> Create(const Ref<MeshData>& meshData);

    private:
        explicit Mesh(const Ref<MeshData>& meshData);

        void BuildVertexArray();

    private:
        Ref<MeshData> m_MeshData;
        Ref<VertexArray> m_VertexArray;
    };
}
