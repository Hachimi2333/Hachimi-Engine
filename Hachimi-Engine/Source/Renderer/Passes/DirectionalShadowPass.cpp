#include "Renderer/Passes/DirectionalShadowPass.h"

#include "Core/Assert.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Shader.h"
#include "Renderer/ShadowMap.h"
#include "Math/Math.h"

#include <array>
#include <limits>

namespace HachimiEngine
{
    namespace
    {
        // Half-extent of the shadow volume around the camera, in world units.
        constexpr float ShadowDistance = 30.0f;

        bool ShouldCastShadows(const DirectionalLight& light)
        {
            return light.CastsShadows
                && light.Intensity > 0.0f
                && Math::Length(light.Direction) > 0.001f;
        }

        Math::Mat4 CalculateLightViewProjection(const RenderView& view, const DirectionalLight& light)
        {
            const Math::Vec3 lightDirection = Math::Normalize(light.Direction);

            // Center the shadow volume between the camera and the area it is looking at.
            const Math::Vec3 center = view.CameraPosition + view.CameraForward * (ShadowDistance * 0.5f);
            const Math::Vec3 lightPosition = center - lightDirection * ShadowDistance;
            const Math::Vec3 upDirection = std::abs(lightDirection.y) > 0.99f
                ? Math::Vec3(1.0f, 0.0f, 0.0f)
                : Math::Vec3(0.0f, 1.0f, 0.0f);

            const Math::Mat4 lightView = Math::LookAt(lightPosition, center, upDirection);

            const std::array<Math::Vec3, 8> corners =
            {
                center + Math::Vec3(-ShadowDistance, -ShadowDistance, -ShadowDistance),
                center + Math::Vec3( ShadowDistance, -ShadowDistance, -ShadowDistance),
                center + Math::Vec3(-ShadowDistance,  ShadowDistance, -ShadowDistance),
                center + Math::Vec3( ShadowDistance,  ShadowDistance, -ShadowDistance),
                center + Math::Vec3(-ShadowDistance, -ShadowDistance,  ShadowDistance),
                center + Math::Vec3( ShadowDistance, -ShadowDistance,  ShadowDistance),
                center + Math::Vec3(-ShadowDistance,  ShadowDistance,  ShadowDistance),
                center + Math::Vec3( ShadowDistance,  ShadowDistance,  ShadowDistance)
            };

            Math::Vec3 minimum(std::numeric_limits<float>::max());
            Math::Vec3 maximum(std::numeric_limits<float>::lowest());
            for (const Math::Vec3& corner : corners)
            {
                const Math::Vec3 lightSpaceCorner = lightView * Math::Vec4(corner, 1.0f);
                minimum = Math::Min(minimum, lightSpaceCorner);
                maximum = Math::Max(maximum, lightSpaceCorner);
            }

            // View-space z is negative in front of the light camera; flip the range for Math::Ortho.
            const Math::Mat4 lightProjection = Math::Ortho(
                minimum.x,
                maximum.x,
                minimum.y,
                maximum.y,
                -maximum.z,
                -minimum.z);

            return lightProjection * lightView;
        }
    }

    void DirectionalShadowPass::Execute(RenderPassContext& context)
    {
        context.DirectionalShadowEnabled = false;

        const DirectionalLight& light = context.View.Lighting.Directional;
        if (!ShouldCastShadows(light))
        {
            return;
        }

        const Ref<Shader> shader = context.Renderers.GetDirectionalShadowShader();
        HE_CORE_ASSERT(shader != nullptr);

        const Math::Mat4 lightViewProjection = CalculateLightViewProjection(context.View, light);

        ShadowMap& shadowMap = context.Renderers.GetShadowMap();
        shadowMap.BindForWriting();

        shader->Bind();
        shader->SetMat4("u_LightViewProjection", lightViewProjection);
        Renderer::SetPolygonOffset(true, 1.0f, 1.0f);

        for (const RenderItem& item : context.View.Items)
        {
            // Line geometry casts no meaningful shadow.
            if (item.Mesh == nullptr || item.Mesh->GetDrawMode() != MeshDrawMode::Triangles)
            {
                continue;
            }

            const Ref<Mesh> gpuMesh = context.Renderers.GetMeshes().GetOrCreate(item.Mesh);
            if (gpuMesh == nullptr)
            {
                continue;
            }

            shader->SetMat4("u_Model", item.Transform);
            Renderer::DrawIndexed(gpuMesh->GetVertexArray(), 0, DrawMode::Triangles);
        }

        Renderer::SetPolygonOffset(false);
        shadowMap.Unbind();

        // The pass that follows reads these two to decide whether to sample the map.
        context.DirectionalLightViewProjection = lightViewProjection;
        context.DirectionalShadowEnabled = true;
    }
}
