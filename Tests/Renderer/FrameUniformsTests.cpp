// FrameUniforms: the CPU side of the FrameBlock uniform buffer.
//
// A std140 mismatch between this struct and Resources/Shaders/Default.glsl is invisible at
// compile time and shows up as garbage lighting, so the layout is pinned here: the header
// carries static_asserts for the offsets, and this suite checks the values that fill them.
// Building the struct needs no OpenGL context.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Renderer/FrameUniforms.h"
#include "Renderer/RenderView.h"
#include "Math/Math.h"

#include <cstddef>
#include <cmath>

using namespace HachimiEngine;

namespace
{
    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    RenderView MakeView(const Math::Mat4& viewProjection)
    {
        RenderView view;
        view.ViewProjection = viewProjection;
        view.CameraPosition = { 1.0f, 2.0f, 3.0f };
        view.Environment.EnvironmentIntensity = 0.75f;
        return view;
    }
}

TEST_SUITE("Renderer")
{
    TEST_CASE("the block lays out as std140 expects")
    {
        // Every member is a mat4 or a vec4, so the offsets are the running sum of 16-byte
        // slots. These are the same numbers the static_asserts in the header pin.
        CHECK(offsetof(FrameUniforms, ViewProjection) == 0);
        CHECK(offsetof(FrameUniforms, DirectionalLightViewProjection) == 64);
        CHECK(offsetof(FrameUniforms, CameraPosition) == 128);
        CHECK(offsetof(FrameUniforms, AmbientColor) == 144);
        CHECK(offsetof(FrameUniforms, DirectionalLightDirection) == 160);
        CHECK(offsetof(FrameUniforms, DirectionalLightColor) == 176);
        CHECK(offsetof(FrameUniforms, PointLightPosition) == 192);
        CHECK(offsetof(FrameUniforms, PointLightColor) == 256);
        CHECK(offsetof(FrameUniforms, SceneParams) == 320);
        CHECK(sizeof(FrameUniforms) == 336);
        CHECK(sizeof(FrameUniforms) % 16 == 0);
    }

    TEST_CASE("camera and light values are packed into the shared vectors")
    {
        const Math::Mat4 viewProjection = Math::Translate(Math::Mat4(1.0f), Math::Vec3(4.0f, 0.0f, 0.0f));
        RenderView view = MakeView(viewProjection);
        view.Lighting.AmbientColor = { 0.1f, 0.2f, 0.3f };
        view.Lighting.AmbientIntensity = 0.5f;
        view.Lighting.Directional.Direction = { 0.0f, -1.0f, 0.0f };
        view.Lighting.Directional.Intensity = 2.5f;
        view.Lighting.Directional.Color = { 1.0f, 0.9f, 0.8f };
        view.Lighting.Directional.ShadowBias = 0.002f;

        const Math::Mat4 lightViewProjection = Math::Scale(Math::Mat4(1.0f), Math::Vec3(2.0f));
        const FrameUniforms frame = FrameUniforms::FromView(view, lightViewProjection, true);

        CHECK(Near(frame.ViewProjection[3].x, 4.0f));
        CHECK(Near(frame.DirectionalLightViewProjection[0].x, 2.0f));

        CHECK(Near(frame.CameraPosition.x, 1.0f));
        CHECK(Near(frame.CameraPosition.y, 2.0f));
        CHECK(Near(frame.CameraPosition.z, 3.0f));

        CHECK(Near(frame.AmbientColor.x, 0.1f));
        CHECK(Near(frame.AmbientColor.w, 0.5f));

        CHECK(Near(frame.DirectionalLightDirection.y, -1.0f));
        CHECK(Near(frame.DirectionalLightDirection.w, 2.5f));

        CHECK(Near(frame.DirectionalLightColor.y, 0.9f));
        CHECK(Near(frame.DirectionalLightColor.w, 0.002f));

        // x = light count, y = shadows, z = environment enabled, w = environment intensity.
        CHECK(Near(frame.SceneParams.y, 1.0f));
        CHECK(Near(frame.SceneParams.z, 1.0f));
        CHECK(Near(frame.SceneParams.w, 0.75f));
    }

    TEST_CASE("point lights fill their slots and the rest stay zeroed")
    {
        RenderView view = MakeView(Math::Mat4(1.0f));
        view.Lighting.PointLightCount = 2;
        view.Lighting.PointLights[0].Position = { 1.0f, 2.0f, 3.0f };
        view.Lighting.PointLights[0].Range = 9.0f;
        view.Lighting.PointLights[0].Color = { 0.5f, 0.25f, 0.125f };
        view.Lighting.PointLights[0].Intensity = 7.0f;
        view.Lighting.PointLights[1].Position = { -1.0f, 0.0f, 0.0f };
        view.Lighting.PointLights[3].Position = { 99.0f, 99.0f, 99.0f };

        const FrameUniforms frame = FrameUniforms::FromView(view, Math::Mat4(1.0f), false);

        CHECK(Near(frame.SceneParams.x, 2.0f));
        CHECK(Near(frame.SceneParams.y, 0.0f));

        CHECK(Near(frame.PointLightPosition[0].x, 1.0f));
        CHECK(Near(frame.PointLightPosition[0].w, 9.0f));
        CHECK(Near(frame.PointLightColor[0].z, 0.125f));
        CHECK(Near(frame.PointLightColor[0].w, 7.0f));

        CHECK(Near(frame.PointLightPosition[1].x, -1.0f));

        // Slot 3 is past the count, so it must not leak the stale position into the shader.
        CHECK(Near(frame.PointLightPosition[3].x, 0.0f));
    }

    TEST_CASE("a point light count beyond the block is clamped")
    {
        RenderView view = MakeView(Math::Mat4(1.0f));
        view.Lighting.PointLightCount = static_cast<int>(LightingEnvironment::MaxPointLights) + 5;

        const FrameUniforms frame = FrameUniforms::FromView(view, Math::Mat4(1.0f), false);

        CHECK(Near(frame.SceneParams.x, static_cast<float>(LightingEnvironment::MaxPointLights)));
    }

    TEST_CASE("a negative point light count is clamped to none")
    {
        RenderView view = MakeView(Math::Mat4(1.0f));
        view.Lighting.PointLightCount = -3;
        view.Lighting.PointLights[0].Position = { 5.0f, 5.0f, 5.0f };

        const FrameUniforms frame = FrameUniforms::FromView(view, Math::Mat4(1.0f), false);

        CHECK(Near(frame.SceneParams.x, 0.0f));
        CHECK(Near(frame.PointLightPosition[0].x, 0.0f));
    }
}
