// MeshFactory and MeshData: built-in primitive geometry, produced without OpenGL.
//
// MeshData holds only CPU vertices, indices and bounds, so every assertion here runs
// headless. This suite is also the regression guard for the seam itself: if geometry
// creation ever starts uploading a vertex array again, constructing these primitives
// would fail in this target.

#include <doctest/doctest.h>

#include "Renderer/MeshData.h"
#include "Renderer/MeshFactory.h"
#include "Math/Math.h"

#include <cmath>

using namespace HachimiEngine;

namespace
{
    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool Near(const Math::Vec3& lhs, const Math::Vec3& rhs, float tolerance = 1e-4f)
    {
        return Near(lhs.x, rhs.x, tolerance) && Near(lhs.y, rhs.y, tolerance) && Near(lhs.z, rhs.z, tolerance);
    }

    // Every triangle of a closed primitive must face outwards, which is what lets the
    // renderer enable backface culling. Winding is checked against the stored normals.
    // The first offending triangle is reported so a failure names the culprit.
    bool TriangleWindingMatchesNormals(const Ref<MeshData>& mesh)
    {
        const std::vector<MeshVertex>& vertices = mesh->GetVertices();
        const std::vector<uint32_t>& indices = mesh->GetIndices();

        if (indices.size() % 3 != 0)
        {
            MESSAGE("index count is not a multiple of three");
            return false;
        }

        for (size_t index = 0; index + 2 < indices.size(); index += 3)
        {
            const MeshVertex& a = vertices[indices[index]];
            const MeshVertex& b = vertices[indices[index + 1]];
            const MeshVertex& c = vertices[indices[index + 2]];

            const Math::Vec3 edge1 = b.Position - a.Position;
            const Math::Vec3 edge2 = c.Position - a.Position;
            const Math::Vec3 geometricNormal = Math::Cross(edge1, edge2);

            // A sphere band ends in pole slivers whose two lower vertices coincide, so
            // the triangle covers no pixels and has no orientation to check. The test
            // for that is relative to the edge lengths, which makes it scale free.
            const float edgeProduct = Math::Length(edge1) * Math::Length(edge2);
            if (edgeProduct <= 0.0f || Math::Length(geometricNormal) < 1e-4f * edgeProduct)
            {
                continue;
            }

            const Math::Vec3 averageNormal = Math::Normalize(a.Normal + b.Normal + c.Normal);
            const float alignment = Math::Dot(Math::Normalize(geometricNormal), averageNormal);
            if (alignment <= 0.0f)
            {
                MESSAGE("triangle " << (index / 3) << " of " << (indices.size() / 3)
                                    << " is wound the wrong way: alignment " << alignment);
                return false;
            }
        }

        return true;
    }
}

TEST_SUITE("Renderer")
{
    TEST_CASE("a cube has axis aligned bounds matching its size")
    {
        const Ref<MeshData> cube = MeshFactory::CreateCube(2.0f);

        REQUIRE(cube.get() != nullptr);
        CHECK(cube->GetDrawMode() == MeshDrawMode::Triangles);
        CHECK(cube->GetVertexCount() == 24);
        CHECK(cube->GetIndexCount() == 36);
        CHECK_FALSE(cube->IsEmpty());

        const Math::AABB& bounds = cube->GetBounds();
        CHECK(bounds.IsValid());
        CHECK(Near(bounds.Min, Math::Vec3(-1.0f)));
        CHECK(Near(bounds.Max, Math::Vec3(1.0f)));
        CHECK(Near(bounds.GetExtents().x, 1.0f));
        CHECK(Near(bounds.GetCenter().y, 0.0f));
    }

    TEST_CASE("the cube faces are wound counter-clockwise seen from outside")
    {
        const Ref<MeshData> cube = MeshFactory::CreateCube();
        REQUIRE(cube.get() != nullptr);
        CHECK(TriangleWindingMatchesNormals(cube));
    }

    TEST_CASE("the plane is wound counter-clockwise seen from above")
    {
        const Ref<MeshData> plane = MeshFactory::CreatePlane();
        REQUIRE(plane.get() != nullptr);
        CHECK(TriangleWindingMatchesNormals(plane));
    }

    TEST_CASE("the sphere is wound counter-clockwise seen from outside")
    {
        const Ref<MeshData> sphere = MeshFactory::CreateSphere(1.0f, 8, 4);
        REQUIRE(sphere.get() != nullptr);
        CHECK(TriangleWindingMatchesNormals(sphere));
    }

    TEST_CASE("a sphere has unit normals and stays inside its radius")
    {
        const Ref<MeshData> sphere = MeshFactory::CreateSphere(2.0f, 16, 8);

        REQUIRE(sphere.get() != nullptr);
        CHECK(sphere->GetVertexCount() == 17 * 9);
        CHECK(sphere->GetIndexCount() == 16 * 8 * 6);

        bool positionsOnRadius = true;
        bool normalsAreUnit = true;
        for (const MeshVertex& vertex : sphere->GetVertices())
        {
            positionsOnRadius = positionsOnRadius && Near(Math::Length(vertex.Position), 2.0f, 1e-3f);
            normalsAreUnit = normalsAreUnit && Near(Math::Length(vertex.Normal), 1.0f, 1e-3f);
        }

        CHECK(positionsOnRadius);
        CHECK(normalsAreUnit);
        CHECK(sphere->GetBounds().Min.y >= -2.0001f);
        CHECK(sphere->GetBounds().Max.y <= 2.0001f);
    }

    TEST_CASE("a plane is flat on the XZ axis and faces up")
    {
        const Ref<MeshData> plane = MeshFactory::CreatePlane(10.0f, 4.0f);

        REQUIRE(plane.get() != nullptr);
        CHECK(plane->GetVertexCount() == 4);
        CHECK(plane->GetIndexCount() == 6);

        bool flat = true;
        bool facesUp = true;
        for (const MeshVertex& vertex : plane->GetVertices())
        {
            flat = flat && Near(vertex.Position.y, 0.0f);
            facesUp = facesUp && Near(vertex.Normal, Math::Vec3(0.0f, 1.0f, 0.0f));
        }

        CHECK(flat);
        CHECK(facesUp);
        CHECK(Near(plane->GetBounds().Min.x, -5.0f));
        CHECK(Near(plane->GetBounds().Max.z, 2.0f));
    }

    TEST_CASE("a grid is line geometry with two vertices per index pair")
    {
        const Ref<MeshData> grid = MeshFactory::CreateGrid(20.0f, 20);

        REQUIRE(grid.get() != nullptr);
        CHECK(grid->GetDrawMode() == MeshDrawMode::Lines);
        CHECK(grid->GetVertexCount() == 2 * 2 * 21);
        CHECK(grid->GetIndexCount() == 2 * 2 * 21);
        CHECK(Near(grid->GetBounds().Min.x, -10.0f));
        CHECK(Near(grid->GetBounds().Max.z, 10.0f));
    }

    TEST_CASE("CreatePrimitive maps every enumerator to geometry")
    {
        CHECK(MeshFactory::CreatePrimitive(PrimitiveMeshType::Cube).get() != nullptr);
        CHECK(MeshFactory::CreatePrimitive(PrimitiveMeshType::Sphere).get() != nullptr);
        CHECK(MeshFactory::CreatePrimitive(PrimitiveMeshType::Plane).get() != nullptr);
        CHECK(MeshFactory::CreatePrimitive(PrimitiveMeshType::Grid).get() != nullptr);
        CHECK(MeshFactory::CreatePrimitive(PrimitiveMeshType::None).get() == nullptr);
    }

    TEST_CASE("mesh data reports empty geometry instead of drawing nothing")
    {
        const Ref<MeshData> empty = MeshData::Create({}, {});
        REQUIRE(empty.get() != nullptr);
        CHECK(empty->IsEmpty());
        CHECK(empty->GetVertexCount() == 0);
        CHECK(empty->GetBounds().IsValid());
    }

    TEST_CASE("the vertex layout matches the MeshVertex field order")
    {
        const BufferLayout layout = MeshVertex::GetLayout();

        REQUIRE(layout.GetElements().size() == 4);
        CHECK(layout.GetElements()[0].Name == "a_Position");
        CHECK(layout.GetElements()[1].Name == "a_Normal");
        CHECK(layout.GetElements()[2].Name == "a_TexCoord");
        CHECK(layout.GetElements()[3].Name == "a_Color");
        CHECK(layout.GetStride() == static_cast<uint32_t>(sizeof(MeshVertex)));
    }
}
