#pragma once

#include "Scene/Entity.h"

#include <sol/sol.hpp>

namespace HachimiEngine
{
    class Scene;

    // Registers the sandboxed HE API on a Lua state.
    // The caller owns the Time table and updates its values every frame.
    void RegisterLuaBindings(sol::state& state, Scene& scene, sol::table& timeTable);

    // Injects one instance's context into its module table before OnCreate. The module table is
    // what the lifecycle callbacks receive as `self`, so this is where `self.entity`, `self.scene`,
    // `self.time` and `self.fields` come from. Fields live in their own sub-table so a script's own
    // globals can never collide with an engine-provided name.
    void AttachLuaScriptInstance(sol::table& moduleTable, Entity entity, Scene& scene, sol::state& state,
                                 sol::table& fieldsTable);
}
