#pragma once

#include "Core/Base.h"
#include "Renderer/Lighting.h"
#include "Renderer/RenderView.h"
#include "Math/Math.h"

#include <array>
#include <cstddef>

namespace HachimiEngine
{
    // Per-view constants, uploaded once per view instead of once per draw.
    //
    // Layout contract with the FrameBlock uniform block in Resources/Shaders/Default.glsl:
    // every member is a mat4 or a vec4, because those are the only types whose std140
    // alignment matches the C++ layout without hand-written padding. A float or vec3 member
    // would silently shift everything after it, so the static assertions below pin the
    // offsets instead of trusting that.
    //
    // Values that naturally belong together share a vec4 and are documented per component:
    // this is the price of the compact layout, and it is paid in one place.
    struct FrameUniforms
    {
        Math::Mat4 ViewProjection { 1.0f };

        Math::Mat4 DirectionalLightViewProjection { 1.0f };

        Math::Vec4 CameraPosition { 0.0f };            // xyz; w unused
        Math::Vec4 AmbientColor { 0.0f };              // rgb; w = intensity
        Math::Vec4 DirectionalLightDirection { 0.0f }; // xyz; w = intensity
        Math::Vec4 DirectionalLightColor { 1.0f };     // rgb; w = shadow bias

        std::array<Math::Vec4, LightingEnvironment::MaxPointLights> PointLightPosition; // xyz; w = range
        std::array<Math::Vec4, LightingEnvironment::MaxPointLights> PointLightColor;    // rgb; w = intensity

        // x = point light count, y = directional shadows enabled, z = environment lighting
        // enabled, w = environment intensity.
        Math::Vec4 SceneParams { 0.0f };

        // Fills every component from one view. Kept next to the layout so the two cannot
        // drift apart.
        static FrameUniforms FromView(const RenderView& view, const Math::Mat4& directionalLightViewProjection,
                                      bool directionalShadowsEnabled);
    };

    // std140 places each mat4 and vec4 on a 16-byte boundary, and the members above are
    // declared in that order, so the C++ offsets must come out as the multiples of 16 below.
    static_assert(offsetof(FrameUniforms, ViewProjection) == 0);
    static_assert(offsetof(FrameUniforms, DirectionalLightViewProjection) == 64);
    static_assert(offsetof(FrameUniforms, CameraPosition) == 128);
    static_assert(offsetof(FrameUniforms, AmbientColor) == 144);
    static_assert(offsetof(FrameUniforms, DirectionalLightDirection) == 160);
    static_assert(offsetof(FrameUniforms, DirectionalLightColor) == 176);
    static_assert(offsetof(FrameUniforms, PointLightPosition) == 192);
    static_assert(offsetof(FrameUniforms, PointLightColor) == 192 + 16 * LightingEnvironment::MaxPointLights);
    static_assert(offsetof(FrameUniforms, SceneParams) == 192 + 32 * LightingEnvironment::MaxPointLights);
    static_assert(sizeof(FrameUniforms) == 208 + 32 * LightingEnvironment::MaxPointLights);
}
