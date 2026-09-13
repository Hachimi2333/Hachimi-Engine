#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Renderer/EnvironmentSettings.h"
#include "Renderer/Lighting.h"
#include "Renderer/MeshData.h"
#include "Math/Math.h"

#include <vector>

namespace HachimiEngine
{
    // One drawable extracted from a scene for a single frame.
    struct RenderItem
    {
        Ref<MeshData> Mesh;
        Math::Mat4 Transform { 1.0f };

        // Surface parameters authored on the entity. The renderer pushes them per draw,
        // so an entity needs no Material instance of its own.
        Math::Vec4 AlbedoColor { 0.8f, 0.8f, 0.82f, 1.0f };
        float Roughness = 0.6f;
        float Metallic = 0.05f;

        // Optional material asset, carried as a reference rather than a resolved object: turning
        // it into a shader and a texture needs the asset database and a GL context, which the
        // render pass has and the scene extraction deliberately does not. This is why building a
        // RenderView stays pure data and works in a headless test.
        AssetHandle Material;
    };

    // Everything the renderer needs for one frame.
    //
    // A scene produces this as plain data and a SceneRenderer consumes it, which is what
    // keeps the scene out of the renderer's state: no scene code writes renderer globals,
    // and two views (editor viewport and game panel) can coexist without interfering.
    struct RenderView
    {
        Math::Mat4 View { 1.0f };
        Math::Mat4 Projection { 1.0f };
        Math::Mat4 ViewProjection { 1.0f };
        Math::Vec3 CameraPosition { 0.0f };
        Math::Vec3 CameraForward { 0.0f, 0.0f, -1.0f };

        LightingEnvironment Lighting;
        EnvironmentSettings Environment;

        std::vector<RenderItem> Items;

        // Editor-only ground grid. Runtime views leave it off.
        bool DrawGrid = false;
    };
}
