#pragma once

#include "Core/Base.h"
#include "Core/Timestep.h"

#include <string_view>

namespace HachimiEngine
{
    class Scene;

    // Phase a system runs in. A frame runs the phases in declaration order, and within a phase
    // the systems run in the order they were added.
    enum class ScenePhase
    {
        PreUpdate = 0,
        FixedUpdate = 1,
        Update = 2,
        LateUpdate = 3
    };

    // One step of the scene simulation.
    //
    // Scene::OnUpdate used to hard-code "physics, then scripts" and bail out entirely when the
    // physics world was missing, so a failed Box3D world silently disabled every script. With
    // systems the scene only decides the order of the phases, and physics and scripting are
    // attached independently of each other.
    class SceneSystem
    {
    public:
        virtual ~SceneSystem() = default;

        // Stable name, used by diagnostics and by Scene::FindSystem.
        virtual std::string_view GetName() const = 0;
        virtual ScenePhase GetPhase() const = 0;

        // Called once when the system is added. Begin work that needs the whole scene here.
        virtual void OnAttach(Scene& scene) { (void)scene; }
        virtual void OnUpdate(Scene& scene, Timestep timestep) = 0;
        // Called once when the system is removed. Tear down in the reverse order of OnAttach.
        virtual void OnDetach(Scene& scene) { (void)scene; }
    };
}
