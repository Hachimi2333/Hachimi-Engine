// Scene::BuildRenderView: the scene-to-renderer seam, checked without a window.
//
// A scene extracts its drawables and lights as plain data, and a SceneRenderer consumes
// that. Nothing here needs an OpenGL context, which is the point: the extraction used to
// live inside the render path and wrote straight into the renderer's global state, so it
// could not be tested at all.

#include <doctest/doctest.h>

#include "Core/Memory.h"
#include "Renderer/MeshData.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/RenderView.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <cmath>

using namespace HachimiEngine;

namespace
{
    SceneRenderDesc MakeDesc(bool drawGrid = false)
    {
        SceneRenderDesc desc;
        desc.View = Math::LookAt(Math::Vec3(0.0f, 0.0f, 10.0f), Math::Vec3(0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
        desc.Projection = Math::Perspective(Math::Radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
        desc.CameraPosition = { 0.0f, 0.0f, 10.0f };
        desc.DrawGrid = drawGrid;
        return desc;
    }

    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("the view carries the camera and the scene environment")
    {
        Scene scene;
        ClearScene(scene);
        scene.GetEnvironmentSettings().Exposure = 2.0f;
        scene.GetEnvironmentSettings().ShowSkybox = false;
        scene.GetEnvironmentSettings().EnvironmentIntensity = 0.25f;

        const SceneRenderDesc desc = MakeDesc(true);
        const RenderView view = scene.BuildRenderView(desc);

        CHECK(Near(view.CameraPosition.z, 10.0f));
        CHECK(Near(view.Environment.Exposure, 2.0f));
        CHECK_FALSE(view.Environment.ShowSkybox);
        CHECK(Near(view.Environment.EnvironmentIntensity, 0.25f));
        CHECK(view.DrawGrid);

        // The view-projection must be the projection applied to the view.
        const Math::Mat4 expected = desc.Projection * desc.View;
        CHECK(Near(view.ViewProjection[3].z, expected[3].z));
        CHECK(Near(view.ViewProjection[3].w, expected[3].w));

        // Looking down -Z from +Z means the camera forward is -Z.
        CHECK(Near(view.CameraForward.z, -1.0f));
    }

    TEST_CASE("invisible and empty meshes are not submitted")
    {
        Scene scene;
        ClearScene(scene);

        Entity visible = scene.CreateEntity("Visible");
        visible.AddComponent<MeshComponent>().Mesh = MeshFactory::CreateCube();

        Entity hidden = scene.CreateEntity("Hidden");
        auto& hiddenMesh = hidden.AddComponent<MeshComponent>();
        hiddenMesh.Mesh = MeshFactory::CreateCube();
        hiddenMesh.Visible = false;

        Entity empty = scene.CreateEntity("Empty");
        empty.AddComponent<MeshComponent>();

        const RenderView view = scene.BuildRenderView(MakeDesc());

        REQUIRE(view.Items.size() == 1);
        CHECK(view.Items[0].Mesh.get() != nullptr);
        CHECK(view.Items[0].Mesh.get() == visible.GetComponent<MeshComponent>().Mesh.get());
    }

    TEST_CASE("an item carries the entity surface values and its world transform")
    {
        Scene scene;
        ClearScene(scene);

        Entity parent = scene.CreateEntity("Parent");
        parent.Transform().Position = { 4.0f, 0.0f, 0.0f };

        Entity child = scene.CreateEntity("Child");
        child.Transform().Position = { 1.0f, 2.0f, 3.0f };
        child.GetComponent<RelationshipComponent>().Parent = parent.GetUUID();

        auto& mesh = child.AddComponent<MeshComponent>();
        mesh.Mesh = MeshFactory::CreateSphere();
        mesh.MaterialColor = { 0.1f, 0.2f, 0.3f, 0.4f };
        mesh.Roughness = 0.35f;
        mesh.Metallic = 0.65f;

        const RenderView view = scene.BuildRenderView(MakeDesc());

        REQUIRE(view.Items.size() == 1);
        const RenderItem& item = view.Items[0];
        CHECK(Near(item.AlbedoColor.x, 0.1f));
        CHECK(Near(item.AlbedoColor.w, 0.4f));
        CHECK(Near(item.Roughness, 0.35f));
        CHECK(Near(item.Metallic, 0.65f));
        CHECK(item.Material.get() == nullptr);
        CHECK(Near(item.Transform[3].x, 5.0f));
        CHECK(Near(item.Transform[3].y, 2.0f));
    }

    TEST_CASE("an explicit material override travels with the item")
    {
        Scene scene;
        ClearScene(scene);

        Entity entity = scene.CreateEntity("Textured");
        auto& mesh = entity.AddComponent<MeshComponent>();
        mesh.Mesh = MeshFactory::CreateCube();
        mesh.MaterialOverride = Material::Create(nullptr);

        const RenderView view = scene.BuildRenderView(MakeDesc());

        REQUIRE(view.Items.size() == 1);
        CHECK(view.Items[0].Material.get() != nullptr);
        CHECK(view.Items[0].Material.get() == mesh.MaterialOverride.get());
    }

    TEST_CASE("a scene with no lights falls back to the default lighting")
    {
        Scene scene;
        ClearScene(scene);

        const RenderView view = scene.BuildRenderView(MakeDesc());

        CHECK(view.Lighting.PointLightCount == 1);
        CHECK(view.Lighting.Directional.Intensity > 0.0f);
    }

    TEST_CASE("a directional light becomes the view's key light")
    {
        Scene scene;
        ClearScene(scene);

        Entity light = scene.CreateEntity("Sun");
        light.Transform().Rotation = { -90.0f, 0.0f, 0.0f };
        auto& lightComponent = light.AddComponent<LightComponent>();
        lightComponent.Type = LightComponent::LightType::Directional;
        lightComponent.Color = { 1.0f, 0.5f, 0.25f };
        lightComponent.Intensity = 3.0f;
        lightComponent.CastsShadows = false;
        lightComponent.ShadowBias = 0.001f;

        const RenderView view = scene.BuildRenderView(MakeDesc());

        CHECK(Near(view.Lighting.Directional.Intensity, 3.0f));
        CHECK(Near(view.Lighting.Directional.Color.y, 0.5f));
        CHECK_FALSE(view.Lighting.Directional.CastsShadows);
        CHECK(Near(view.Lighting.Directional.ShadowBias, 0.001f));
        CHECK(view.Lighting.PointLightCount == 0);

        // A light entity points along its local -Z, so rotating -90 degrees around X
        // aims it straight down: the usual "sun from above".
        CHECK(Near(Math::Length(view.Lighting.Directional.Direction), 1.0f, 1e-3f));
        CHECK(Near(view.Lighting.Directional.Direction.y, -1.0f, 1e-3f));
    }

    TEST_CASE("point lights beyond the shader's array are dropped")
    {
        Scene scene;
        ClearScene(scene);

        const size_t lightCount = LightingEnvironment::MaxPointLights + 3;
        for (size_t index = 0; index < lightCount; ++index)
        {
            Entity light = scene.CreateEntity("Point Light");
            light.Transform().Position = { static_cast<float>(index), 1.0f, 0.0f };
            auto& lightComponent = light.AddComponent<LightComponent>();
            lightComponent.Type = LightComponent::LightType::Point;
            lightComponent.Intensity = static_cast<float>(index) + 1.0f;
        }

        const RenderView view = scene.BuildRenderView(MakeDesc());

        CHECK(view.Lighting.PointLightCount == static_cast<int>(LightingEnvironment::MaxPointLights));
        CHECK(view.Lighting.PointLights[0].Intensity > 0.0f);
    }

    TEST_CASE("building a view does not modify the scene")
    {
        Scene scene;
        ClearScene(scene);

        Entity entity = scene.CreateEntity("Cube");
        auto& mesh = entity.AddComponent<MeshComponent>();
        mesh.Mesh = MeshFactory::CreateCube();

        // Extraction is const and must not materialize anything behind the caller's back:
        // a mesh entity keeps its empty material override, and building twice is stable.
        const RenderView first = scene.BuildRenderView(MakeDesc());
        const RenderView second = scene.BuildRenderView(MakeDesc());

        CHECK(mesh.MaterialOverride.get() == nullptr);
        CHECK(first.Items.size() == second.Items.size());
        CHECK(first.Items.size() == 1);
    }
}
