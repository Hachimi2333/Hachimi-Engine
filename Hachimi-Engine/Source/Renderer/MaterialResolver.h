#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>
#include <unordered_map>

namespace HachimiEngine
{
    class Material;
    class RendererContext;

    // Turns a material asset handle into a drawable Material.
    //
    // This is where the asset layer meets the renderer: a RenderItem only carries the handle,
    // Scene::BuildRenderView stays pure data, and the GPU resources are built here, on the
    // render path, exactly like MeshLibrary does for geometry. It lives on the RendererContext
    // because it needs the shader library, the texture cache and the asset database at once.
    class MaterialResolver
    {
    public:
        explicit MaterialResolver(RendererContext& renderers);
        ~MaterialResolver();

        MaterialResolver(const MaterialResolver&) = delete;
        MaterialResolver& operator=(const MaterialResolver&) = delete;

        // Returns nullptr for an invalid handle, an asset that is gone, or a build with no asset
        // database; the caller then draws with the component's inline values.
        Ref<Material> Resolve(const AssetHandle& handle);

        // Drops every resolved material, e.g. when the asset database was reloaded.
        void Clear();

    private:
        struct Entry
        {
            Ref<Material> Material;
            uint64_t Revision = 0;
        };

        RendererContext& m_Renderers;
        std::unordered_map<AssetHandle, Entry> m_Cache;
    };
}
