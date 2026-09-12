#include "Renderer/FrameUniforms.h"

namespace HachimiEngine
{
    FrameUniforms FrameUniforms::FromView(
        const RenderView& view,
        const Math::Mat4& directionalLightViewProjection,
        bool directionalShadowsEnabled)
    {
        const LightingEnvironment& lighting = view.Lighting;

        FrameUniforms uniforms;
        uniforms.ViewProjection = view.ViewProjection;
        uniforms.DirectionalLightViewProjection = directionalLightViewProjection;

        uniforms.CameraPosition = Math::Vec4(view.CameraPosition, 0.0f);
        uniforms.AmbientColor = Math::Vec4(lighting.AmbientColor, lighting.AmbientIntensity);
        uniforms.DirectionalLightDirection = Math::Vec4(lighting.Directional.Direction, lighting.Directional.Intensity);
        uniforms.DirectionalLightColor = Math::Vec4(lighting.Directional.Color, lighting.Directional.ShadowBias);

        uniforms.PointLightPosition.fill(Math::Vec4(0.0f));
        uniforms.PointLightColor.fill(Math::Vec4(0.0f));

        const size_t lightCount = static_cast<size_t>(Math::Clamp(
            lighting.PointLightCount,
            0,
            static_cast<int>(LightingEnvironment::MaxPointLights)));

        for (size_t index = 0; index < lightCount; ++index)
        {
            const PointLight& pointLight = lighting.PointLights[index];
            uniforms.PointLightPosition[index] = Math::Vec4(pointLight.Position, pointLight.Range);
            uniforms.PointLightColor[index] = Math::Vec4(pointLight.Color, pointLight.Intensity);
        }

        uniforms.SceneParams = Math::Vec4(
            static_cast<float>(lightCount),
            directionalShadowsEnabled ? 1.0f : 0.0f,
            view.Environment.EnvironmentIntensity > 0.0f ? 1.0f : 0.0f,
            view.Environment.EnvironmentIntensity);

        return uniforms;
    }
}
