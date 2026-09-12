#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/MeshData.h"

namespace HachimiEngine
{
    enum class PrimitiveMeshType
    {
        None = 0,
        Cube = 1,
        Sphere = 2,
        Plane = 3,
        Grid = 4
    };

    // Creates the built-in primitive geometry used before external model import is
    // implemented. The result is CPU-only, so this never needs an OpenGL context.
    class MeshFactory
    {
    public:
        static Ref<MeshData> CreateCube(float size = 1.0f);
        static Ref<MeshData> CreateSphere(float radius = 0.5f, uint32_t sectorCount = 32, uint32_t stackCount = 16);
        static Ref<MeshData> CreatePlane(float width = 10.0f, float height = 10.0f);
        static Ref<MeshData> CreateGrid(float size = 20.0f, uint32_t divisions = 20);

        static Ref<MeshData> CreatePrimitive(PrimitiveMeshType type);
    };
}
