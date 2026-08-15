#include "Renderer/MeshFactory.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <utility>

namespace HachimiEngine
{
    Ref<Mesh> MeshFactory::CreateCube(float size)
    {
        const float half = size * 0.5f;
        const glm::vec4 color(0.82f, 0.82f, 0.86f, 1.0f);

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

        const std::vector<uint32_t> indices =
        {
             0,  1,  2,  0,  2,  3,
             4,  5,  6,  4,  6,  7,
             8,  9, 10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };

        return Mesh::Create(std::move(vertices), std::move(indices));
    }

    Ref<Mesh> MeshFactory::CreateSphere(float radius, uint32_t sectorCount, uint32_t stackCount)
    {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;

        for (uint32_t y = 0; y <= stackCount; ++y)
        {
            const float stackPosition = static_cast<float>(y) / static_cast<float>(stackCount);
            const float phi = stackPosition * glm::pi<float>();

            for (uint32_t x = 0; x <= sectorCount; ++x)
            {
                const float sectorPosition = static_cast<float>(x) / static_cast<float>(sectorCount);
                const float theta = sectorPosition * glm::two_pi<float>();

                const float sinPhi = std::sin(phi);
                const glm::vec3 position(
                    radius * sinPhi * std::cos(theta),
                    radius * std::cos(phi),
                    radius * sinPhi * std::sin(theta));

                MeshVertex vertex;
                vertex.Position = position;
                vertex.Normal = glm::normalize(position);
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

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(first + 1);
                indices.push_back(second);
                indices.push_back(second + 1);
            }
        }

        return Mesh::Create(std::move(vertices), std::move(indices));
    }

    Ref<Mesh> MeshFactory::CreatePlane(float width, float height)
    {
        const float halfWidth = width * 0.5f;
        const float halfHeight = height * 0.5f;
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec4 color(0.75f, 0.75f, 0.78f, 1.0f);

        const std::vector<MeshVertex> vertices =
        {
            { { -halfWidth, 0.0f, -halfHeight }, normal, { 0.0f, 0.0f }, color },
            { {  halfWidth, 0.0f, -halfHeight }, normal, { 1.0f, 0.0f }, color },
            { {  halfWidth, 0.0f,  halfHeight }, normal, { 1.0f, 1.0f }, color },
            { { -halfWidth, 0.0f,  halfHeight }, normal, { 0.0f, 1.0f }, color }
        };

        const std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };
        return Mesh::Create(std::move(vertices), std::move(indices));
    }

    Ref<Mesh> MeshFactory::CreateGrid(float size, uint32_t divisions)
    {
        const float halfSize = size * 0.5f;
        const float step = size / static_cast<float>(divisions);
        const glm::vec4 color(0.35f, 0.35f, 0.38f, 1.0f);

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

        return Mesh::Create(std::move(vertices), std::move(indices), MeshDrawMode::Lines);
    }

    Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveMeshType type)
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
