#pragma once

#include "Asset/AssetHandle.h"
#include "Scene/ComponentRegistry.h"
#include "Renderer/MeshData.h"
#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

namespace HachimiEngine
{
    // Everything needed to draw one entity: which primitive, how it is shaded, whether it is
    // visible at all.
    //
    // The surface values live on the component rather than only in a material asset, so an entity
    // is self-describing without one: with no material assigned it shades with exactly these
    // numbers, and with one assigned they stay as the fallback for what the material does not
    // specify. That is what makes "create a cube and colour it" work with no asset pipeline at
    // all, while a shared material still behaves like a shared material.
    struct MeshRendererComponent
    {
        // CPU geometry. Rebuilt from Primitive, never serialized.
        Ref<MeshData> Mesh;
        PrimitiveMeshType Primitive = PrimitiveMeshType::Cube;

        // Optional material asset. Invalid means "shade with the values below".
        AssetHandle Material;

        Math::Vec4 AlbedoColor { 0.8f, 0.8f, 0.82f, 1.0f };
        float Roughness = 0.6f;
        float Metallic = 0.05f;
        bool Visible = true;

        // Replaces the geometry with the mesh a primitive names, so changing the enum and changing
        // the mesh cannot drift apart. Every path that sets the primitive - the descriptor's
        // default, the inspector, deserialization, undo - goes through this rather than assigning
        // the enum and forgetting the geometry. Returns the component so a caller can keep going.
        MeshRendererComponent& SetPrimitive(PrimitiveMeshType primitive);
    };

    ComponentDescriptor MakeMeshRendererComponentDescriptor();
}
