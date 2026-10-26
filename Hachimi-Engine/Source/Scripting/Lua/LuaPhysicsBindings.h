#pragma once

#include <sol/sol.hpp>

namespace HachimiEngine
{
    class Scene;
    void RegisterLuaPhysicsBindings(sol::state& state, sol::table& he, Scene& scene);
}
