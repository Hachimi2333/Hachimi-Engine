#pragma once

#include "Scene/ComponentRegistry.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    // Drawable geometry plus the surface values the renderer shades it with.
    //
    // The values live on the entity rather than in a Material instance: the renderer reads them
    // into a RenderItem and pushes them per draw, so an entity needs no GPU-side object of its
    // own. MaterialOverride stays empty unless a material asset is assigned, and contributes the
    // shader and the albedo texture when it is.
    struct MeshComponent
    {
        Ref<MeshData> Mesh;
        PrimitiveMeshType PrimitiveType = PrimitiveMeshType::Cube;
        Ref<Material> MaterialOverride;
        Math::Vec4 MaterialColor { 0.8f, 0.8f, 0.82f, 1.0f };
        float Roughness = 0.6f;
        float Metallic = 0.05f;
        bool Visible = true;
    };

    ComponentDescriptor MakeMeshComponentDescriptor();
}
