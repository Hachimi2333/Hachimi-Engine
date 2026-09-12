// Math: the GLM wrapper the engine and editor use instead of including GLM directly.

#include <doctest/doctest.h>

#include "Math/Math.h"

#include <cmath>

using namespace HachimiEngine;

namespace
{
    // Vectors and matrices are compared through their difference rather than element by
    // element, so a single assertion covers the whole value.
    bool Near(const Math::Vec3& lhs, const Math::Vec3& rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs.x - rhs.x) <= tolerance && std::abs(lhs.y - rhs.y) <= tolerance
            && std::abs(lhs.z - rhs.z) <= tolerance;
    }

    bool NearZero(const Math::Vec3& value, float tolerance = 1e-4f)
    {
        return Near(value, Math::Vec3(0.0f), tolerance);
    }

    bool NearIdentity(const Math::Mat4& matrix, float tolerance = 1e-4f)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                const float expected = column == row ? 1.0f : 0.0f;
                if (std::abs(matrix[column][row] - expected) > tolerance)
                {
                    return false;
                }
            }
        }
        return true;
    }
}

TEST_SUITE_BEGIN("Math");

TEST_CASE("angle helpers convert both ways")
{
    CHECK(Math::Degrees(Math::Pi<float>()) == doctest::Approx(180.0f));
    CHECK(Math::Radians(180.0f) == doctest::Approx(Math::Pi<float>()));
    CHECK(Math::TwoPi<float>() == doctest::Approx(2.0f * Math::Pi<float>()));

    const float angle = 37.5f;
    CHECK(Math::Degrees(Math::Radians(angle)) == doctest::Approx(angle));
}

TEST_CASE("vector helpers keep their documented identities")
{
    const Math::Vec3 x(1.0f, 0.0f, 0.0f);
    const Math::Vec3 y(0.0f, 1.0f, 0.0f);
    const Math::Vec3 diagonal(3.0f, 4.0f, 0.0f);

    CHECK(Math::Length(diagonal) == doctest::Approx(5.0f));
    CHECK(Math::Length(Math::Normalize(diagonal)) == doctest::Approx(1.0f));
    CHECK(Math::Dot(x, y) == doctest::Approx(0.0f));
    CHECK(Near(Math::Cross(x, y), Math::Vec3(0.0f, 0.0f, 1.0f)));
    CHECK(Near(Math::Cross(y, x), Math::Vec3(0.0f, 0.0f, -1.0f)));
    CHECK(NearZero(Math::Cross(x, x)));
    CHECK(Math::Min(2.0f, 5.0f) == doctest::Approx(2.0f));
    CHECK(Math::Max(2.0f, 5.0f) == doctest::Approx(5.0f));
    CHECK(Math::Clamp(12.0f, 0.0f, 10.0f) == doctest::Approx(10.0f));
    CHECK(Math::Clamp(-3.0f, 0.0f, 10.0f) == doctest::Approx(0.0f));
    CHECK(Math::Clamp(4.0f, 0.0f, 10.0f) == doctest::Approx(4.0f));
    CHECK(Math::Mix(0.0f, 10.0f, 0.25f) == doctest::Approx(2.5f));
}

TEST_CASE("matrix helpers compose and invert")
{
    const Math::Mat4 identity(1.0f);
    const Math::Mat4 translated = Math::Translate(identity, Math::Vec3(1.0f, 2.0f, 3.0f));
    const Math::Mat4 scaled = Math::Scale(identity, Math::Vec3(2.0f, 2.0f, 2.0f));
    const Math::Mat4 combined = translated * scaled;

    CHECK(NearIdentity(identity));
    CHECK(NearIdentity(combined * Math::Inverse(combined)));
    CHECK(NearIdentity(Math::Transpose(identity)));

    // The translation lands in the fourth column, the scale on the diagonal.
    CHECK(combined[3][0] == doctest::Approx(1.0f));
    CHECK(combined[3][1] == doctest::Approx(2.0f));
    CHECK(combined[3][2] == doctest::Approx(3.0f));
    CHECK(combined[0][0] == doctest::Approx(2.0f));
    CHECK(combined[1][1] == doctest::Approx(2.0f));
    CHECK(combined[2][2] == doctest::Approx(2.0f));
}

TEST_CASE("quaternion rotation matches its matrix form")
{
    // glm::quat's (w, xyz) constructor takes the half-angle form, so a quarter turn about
    // +Y is (cos 45, (0, sin 45, 0)).
    const float halfAngle = Math::Radians(45.0f);
    const Math::Quat rotation(std::cos(halfAngle), Math::Vec3(0.0f, std::sin(halfAngle), 0.0f));

    // A quarter turn about +Y maps +X onto -Z.
    CHECK(Near(Math::Rotate(rotation, Math::Vec3(1.0f, 0.0f, 0.0f)), Math::Vec3(0.0f, 0.0f, -1.0f)));

    const Math::Mat4 asMatrix = Math::ToMat4(rotation);
    const Math::Vec3 rotatedByMatrix = Math::Vec3(asMatrix * Math::Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    CHECK(Near(rotatedByMatrix, Math::Vec3(0.0f, 0.0f, -1.0f)));

    const Math::Quat fromMatrix = Math::QuatCast(Math::Mat3(asMatrix));
    CHECK(Near(Math::Rotate(fromMatrix, Math::Vec3(1.0f, 0.0f, 0.0f)), Math::Vec3(0.0f, 0.0f, -1.0f)));

    // The quaternion is built from half-angle cosines in float, so the angle recovered
    // from it is only accurate to a fraction of a degree.
    const Math::Vec3 euler = Math::EulerAngles(rotation);
    CHECK(std::abs(euler.y) == doctest::Approx(Math::Radians(90.0f)).epsilon(1e-3));
}

TEST_CASE("projection and view matrices stay finite")
{
    const Math::Mat4 perspective = Math::Perspective(Math::Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const Math::Mat4 ortho = Math::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    const Math::Mat4 view = Math::LookAt(Math::Vec3(0.0f, 0.0f, 5.0f), Math::Vec3(0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));

    for (const Math::Mat4* matrix : { &perspective, &ortho, &view })
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                CHECK(std::isfinite((*matrix)[column][row]));
            }
        }
    }

    // W is the near/far mapping row for a standard perspective projection.
    CHECK(perspective[3][3] == doctest::Approx(0.0f));
    CHECK(perspective[2][2] < 0.0f);
}

TEST_CASE("ValuePtr exposes the matrix storage")
{
    const Math::Mat4 identity(1.0f);
    const float* data = Math::ValuePtr(identity);

    CHECK(data[0] == doctest::Approx(1.0f));
    CHECK(data[5] == doctest::Approx(1.0f));
    CHECK(data[10] == doctest::Approx(1.0f));
    CHECK(data[15] == doctest::Approx(1.0f));
    CHECK(data[1] == doctest::Approx(0.0f));
}

TEST_SUITE_END();
