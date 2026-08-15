#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/Mesh.h"

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

    // Creates built-in primitive meshes used before external model import is implemented.
    class MeshFactory
    {
    public:
        static Ref<Mesh> CreateCube(float size = 1.0f);
        static Ref<Mesh> CreateSphere(float radius = 0.5f, uint32_t sectorCount = 32, uint32_t stackCount = 16);
        static Ref<Mesh> CreatePlane(float width = 10.0f, float height = 10.0f);
        static Ref<Mesh> CreateGrid(float size = 20.0f, uint32_t divisions = 20);

        static Ref<Mesh> CreatePrimitive(PrimitiveMeshType type);
    };
}
