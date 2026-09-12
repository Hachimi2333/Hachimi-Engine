#include "Renderer/MeshLibrary.h"

#include "Renderer/Mesh.h"
#include "Renderer/MeshData.h"

#include <utility>

namespace HachimiEngine
{
    Ref<Mesh> MeshLibrary::GetOrCreate(const Ref<MeshData>& meshData)
    {
        if (meshData == nullptr || meshData->IsEmpty())
        {
            return nullptr;
        }

        // The key is the MeshData address. It is stable for the lifetime of the
        // geometry, because MeshData is only ever held through Ref.
        const MeshData* key = meshData.get();
        const auto existing = m_GpuMeshes.find(key);
        if (existing != m_GpuMeshes.end() && existing->second.GpuMesh != nullptr)
        {
            return existing->second.GpuMesh;
        }

        Ref<Mesh> gpuMesh = Mesh::Create(meshData);
        if (gpuMesh == nullptr)
        {
            return nullptr;
        }

        Entry& entry = m_GpuMeshes[key];
        entry.Source = meshData;
        entry.GpuMesh = std::move(gpuMesh);

        if (m_GpuMeshes.size() > SweepThreshold)
        {
            CollectGarbage();
        }

        // CollectGarbage only erases entries whose MeshData expired, and this call
        // holds a reference to it, so the entry returned here is still alive.
        return entry.GpuMesh;
    }

    void MeshLibrary::Clear()
    {
        m_GpuMeshes.clear();
    }

    void MeshLibrary::CollectGarbage()
    {
        std::erase_if(m_GpuMeshes, [](const auto& entry) { return entry.second.Source.expired(); });
    }
}
