#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

#include <cmath>
#include <utility>

namespace HachimiEngine
{
    Ref<MeshData> MeshFactory::CreateCube(float size)
    {
        const float half = size * 0.5f;
        const Math::Vec4 color(0.82f, 0.82f, 0.86f, 1.0f);

        std::vector<MeshVertex> vertices =
        {
            // +X face
            { { half, -half,  half }, { 1, 0, 0 }, { 0, 0 }, color },
            { { half,  half,  half }, { 1, 0, 0 }, { 1, 0 }, color },
            { { half,  half, -half }, { 1, 0, 0 }, { 1, 1 }, color },
            { { half, -half, -half }, { 1, 0, 0 }, { 0, 1 }, color },
            // -X face
            { {-half, -half, -half }, {-1, 0, 0 }, { 0, 0 }, color },
            { {-half,  half, -half }, {-1, 0, 0 }, { 1, 0 }, color },
            { {-half,  half,  half }, {-1, 0, 0 }, { 1, 1 }, color },
            { {-half, -half,  half }, {-1, 0, 0 }, { 0, 1 }, color },
            // +Y face
            { {-half,  half,  half }, { 0, 1, 0 }, { 0, 0 }, color },
            { { half,  half,  half }, { 0, 1, 0 }, { 1, 0 }, color },
            { { half,  half, -half }, { 0, 1, 0 }, { 1, 1 }, color },
            { {-half,  half, -half }, { 0, 1, 0 }, { 0, 1 }, color },
            // -Y face
            { {-half, -half, -half }, { 0,-1, 0 }, { 0, 0 }, color },
            { { half, -half, -half }, { 0,-1, 0 }, { 1, 0 }, color },
            { { half, -half,  half }, { 0,-1, 0 }, { 1, 1 }, color },
            { {-half, -half,  half }, { 0,-1, 0 }, { 0, 1 }, color },
            // +Z face
            { {-half, -half,  half }, { 0, 0, 1 }, { 0, 0 }, color },
            { { half, -half,  half }, { 0, 0, 1 }, { 1, 0 }, color },
            { { half,  half,  half }, { 0, 0, 1 }, { 1, 1 }, color },
            { {-half,  half,  half }, { 0, 0, 1 }, { 0, 1 }, color },
            // -Z face
            { { half, -half, -half }, { 0, 0,-1 }, { 0, 0 }, color },
            { {-half, -half, -half }, { 0, 0,-1 }, { 1, 0 }, color },
            { {-half,  half, -half }, { 0, 0,-1 }, { 1, 1 }, color },
            { { half,  half, -half }, { 0, 0,-1 }, { 0, 1 }, color }
        };

        // Counter-clockwise seen from outside, which is the front face the renderer
        // culls against. The +X and -X faces are listed in the opposite order of the
        // other four because their quad corners run the other way round.
        const std::vector<uint32_t> indices =
        {
             0,  2,  1,  0,  3,  2, // +X
             4,  6,  5,  4,  7,  6, // -X
             8,  9, 10,  8, 10, 11, // +Y
            12, 13, 14, 12, 14, 15, // -Y
            16, 17, 18, 16, 18, 19, // +Z
            20, 21, 22, 20, 22, 23  // -Z
        };

        return MeshData::Create(std::move(vertices), std::move(indices));
    }

    Ref<MeshData> MeshFactory::CreateSphere(float radius, uint32_t sectorCount, uint32_t stackCount)
    {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;

        for (uint32_t y = 0; y <= stackCount; ++y)
        {
            const float stackPosition = static_cast<float>(y) / static_cast<float>(stackCount);
            const float phi = stackPosition * Math::Pi<float>();

            for (uint32_t x = 0; x <= sectorCount; ++x)
            {
                const float sectorPosition = static_cast<float>(x) / static_cast<float>(sectorCount);
                const float theta = sectorPosition * Math::TwoPi<float>();

                const float sinPhi = std::sin(phi);
                const Math::Vec3 position(
                    radius * sinPhi * std::cos(theta),
                    radius * std::cos(phi),
                    radius * sinPhi * std::sin(theta));

                MeshVertex vertex;
                vertex.Position = position;
                vertex.Normal = Math::Normalize(position);
                vertex.TexCoord = { sectorPosition, 1.0f - stackPosition };
                vertex.Color = { 0.80f, 0.80f, 0.84f, 1.0f };
                vertices.push_back(vertex);
            }
        }

        for (uint32_t y = 0; y < stackCount; ++y)
        {
            for (uint32_t x = 0; x < sectorCount; ++x)
            {
                const uint32_t first = y * (sectorCount + 1) + x;
                const uint32_t second = first + sectorCount + 1;

                // Counter-clockwise seen from outside the sphere.
                indices.push_back(first);
                indices.push_back(first + 1);
                indices.push_back(second);

                indices.push_back(first + 1);
                indices.push_back(second + 1);
                indices.push_back(second);
            }
        }

        return MeshData::Create(std::move(vertices), std::move(indices));
    }

    Ref<MeshData> MeshFactory::CreatePlane(float width, float height)
    {
        const float halfWidth = width * 0.5f;
        const float halfHeight = height * 0.5f;
        const Math::Vec3 upNormal(0.0f, 1.0f, 0.0f);
        const Math::Vec3 downNormal(0.0f, -1.0f, 0.0f);
        const Math::Vec4 color(0.75f, 0.75f, 0.78f, 1.0f);

        // The plane is the engine's floor primitive, so it carries both faces: a single-sided
        // quad disappears as soon as the camera drops below it.
        const std::vector<MeshVertex> vertices =
        {
            // Upward face, counter-clockwise seen from above.
            { { -halfWidth, 0.0f, -halfHeight }, upNormal, { 0.0f, 0.0f }, color },
            { {  halfWidth, 0.0f, -halfHeight }, upNormal, { 1.0f, 0.0f }, color },
            { {  halfWidth, 0.0f,  halfHeight }, upNormal, { 1.0f, 1.0f }, color },
            { { -halfWidth, 0.0f,  halfHeight }, upNormal, { 0.0f, 1.0f }, color },
            // Downward face, counter-clockwise seen from below.
            { { -halfWidth, 0.0f, -halfHeight }, downNormal, { 0.0f, 0.0f }, color },
            { { -halfWidth, 0.0f,  halfHeight }, downNormal, { 0.0f, 1.0f }, color },
            { {  halfWidth, 0.0f,  halfHeight }, downNormal, { 1.0f, 1.0f }, color },
            { {  halfWidth, 0.0f, -halfHeight }, downNormal, { 1.0f, 0.0f }, color }
        };

        const std::vector<uint32_t> indices =
        {
             0, 2, 1, 0, 3, 2, // up
             4, 6, 5, 4, 7, 6  // down
        };
        return MeshData::Create(std::move(vertices), std::move(indices));
    }

    Ref<MeshData> MeshFactory::CreateGrid(float size, uint32_t divisions)
    {
        const float halfSize = size * 0.5f;
        const float step = size / static_cast<float>(divisions);
        const Math::Vec4 color(0.35f, 0.35f, 0.38f, 1.0f);

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;

        uint32_t vertexIndex = 0;
        for (uint32_t i = 0; i <= divisions; ++i)
        {
            const float coordinate = -halfSize + step * static_cast<float>(i);

            vertices.push_back({ { coordinate, 0.0f, -halfSize }, { 0, 1, 0 }, { 0, 0 }, color });
            vertices.push_back({ { coordinate, 0.0f,  halfSize }, { 0, 1, 0 }, { 0, 0 }, color });
            indices.push_back(vertexIndex++);
            indices.push_back(vertexIndex++);

            vertices.push_back({ { -halfSize, 0.0f, coordinate }, { 0, 1, 0 }, { 0, 0 }, color });
            vertices.push_back({ {  halfSize, 0.0f, coordinate }, { 0, 1, 0 }, { 0, 0 }, color });
            indices.push_back(vertexIndex++);
            indices.push_back(vertexIndex++);
        }

        return MeshData::Create(std::move(vertices), std::move(indices), MeshDrawMode::Lines);
    }

    Ref<MeshData> MeshFactory::CreatePrimitive(PrimitiveMeshType type)
    {
        switch (type)
        {
            case PrimitiveMeshType::Cube:
                return CreateCube();
            case PrimitiveMeshType::Sphere:
                return CreateSphere();
            case PrimitiveMeshType::Plane:
                return CreatePlane();
            case PrimitiveMeshType::Grid:
                return CreateGrid();
            case PrimitiveMeshType::None:
                break;
        }

        return nullptr;
    }
}
