#pragma once
#include "Core/Memory.h"
#include "Editor/EditorCommand.h"
namespace HachimiEngine::SceneEnvironmentCommands
{
    Scope<EditorCommand> MakeSetSkybox(bool before, bool after);
    Scope<EditorCommand> MakeSetExposure(float before, float after);
    Scope<EditorCommand> MakeSetIntensity(float before, float after);
}
