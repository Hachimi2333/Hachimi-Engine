#include "Renderer/MeshData.h"

#include <limits>
#include <utility>

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

    MeshData::MeshData(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode)
        : m_Vertices(std::move(vertices)), m_Indices(std::move(indices)), m_DrawMode(drawMode)
    {
        RecalculateBounds();
    }

    void MeshData::RecalculateBounds()
    {
        if (m_Vertices.empty())
        {
            m_Bounds = Math::AABB {};
            return;
        }

        Math::Vec3 minimum(std::numeric_limits<float>::max());
        Math::Vec3 maximum(std::numeric_limits<float>::lowest());

        for (const MeshVertex& vertex : m_Vertices)
        {
            minimum = Math::Min(minimum, vertex.Position);
            maximum = Math::Max(maximum, vertex.Position);
        }

        m_Bounds.Min = minimum;
        m_Bounds.Max = maximum;
    }

    Ref<MeshData> MeshData::Create(std::vector<MeshVertex> vertices, std::vector<uint32_t> indices, MeshDrawMode drawMode)
    {
        return CreateRef<MeshData>(std::move(vertices), std::move(indices), drawMode);
    }
}
