#pragma once

#include "Scene/Entity.h"

#include <sol/sol.hpp>

namespace HachimiEngine
{
    class Scene;

    // Registers the sandboxed HE API on a Lua state.
    // The caller owns the Time table and updates its values every frame.
    void RegisterLuaBindings(sol::state& state, Scene& scene, sol::table& timeTable);

    // Injects the entity/scene context into a script module table before OnCreate.
    void AttachLuaScriptInstance(sol::table& moduleTable, Entity entity, Scene& scene);
}
