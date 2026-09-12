#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstddef>
#include <unordered_map>

namespace HachimiEngine
{
    class Mesh;
    class MeshData;

    // Main-thread-only cache that turns CPU MeshData into uploaded GPU Mesh objects.
    //
    // A MeshData is uploaded the first time it is drawn and reused from then on, so
    // N entities sharing one primitive cost one vertex array. Entries whose MeshData
    // has been released are dropped lazily, so the cache never keeps scene geometry
    // alive and never grows without bound.
    //
    // Every method issues OpenGL calls through Mesh, so it must run with a context
    // current. Headless code that only needs geometry uses MeshData instead.
    class MeshLibrary
    {
    public:
        // Returns nullptr when the geometry is null or empty.
        Ref<Mesh> GetOrCreate(const Ref<MeshData>& meshData);

        // Releases every uploaded mesh. Requires a current OpenGL context.
        void Clear();

        // Drops the entries whose MeshData is gone. Runs automatically once the cache
        // has grown past its sweep threshold, so callers rarely need it directly.
        void CollectGarbage();

        size_t GetMeshCount() const { return m_GpuMeshes.size(); }

    private:
        struct Entry
        {
            std::weak_ptr<MeshData> Source;
            Ref<Mesh> GpuMesh;
        };

        // Sweeping is amortized on insert: a lookup never walks the whole cache.
        static constexpr size_t SweepThreshold = 64;

        std::unordered_map<const MeshData*, Entry> m_GpuMeshes;
    };
}
