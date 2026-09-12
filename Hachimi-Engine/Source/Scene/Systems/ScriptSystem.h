#pragma once

#include "Core/Memory.h"
#include "Scene/SceneSystem.h"

namespace HachimiEngine
{
    class ScriptWorld;

    // Runs the scene's script instances.
    //
    // Scripts run on the variable update phase, independently of whether physics is available:
    // a scene with no Box3D world can still drive its entities from Lua.
    class ScriptSystem final : public SceneSystem
    {
    public:
        std::string_view GetName() const override { return "Scripts"; }
        ScenePhase GetPhase() const override { return ScenePhase::Update; }

        void OnAttach(Scene& scene) override;
        void OnUpdate(Scene& scene, Timestep timestep) override;
        void OnDetach(Scene& scene) override;

        bool IsRunning() const;

    private:
        Scope<ScriptWorld> m_ScriptWorld;
    };
}
