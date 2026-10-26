#pragma once

#include "Core/Memory.h"
#include "Renderer/DebugDraw.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/SceneRenderTarget.h"

namespace HachimiEngine
{
    // Rendering-only state. None of these resources exist during a headless runtime session.
    struct SceneRuntimePresentation
    {
        Scope<SceneRenderer> Renderer;
        Scope<SceneRenderTarget> Target;
        Scope<DebugDraw> Debug;
    };
}
