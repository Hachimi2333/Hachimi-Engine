#pragma once

#include "Core/Memory.h"
#include "Scene/SceneSystem.h"

namespace HachimiEngine
{
    // Runs the scene's script instances.
    //
    // Scripts run on the variable update phase, independently of whether physics is available:
    // a scene with no Box3D world can still drive its entities from Lua.
    //
    // The script world is held behind an opaque implementation pointer, so this header does not
    // depend on the scripting backend and a translation unit that only attaches the system does
    // not have to see the whole Lua layer.
    class ScriptSystem final : public SceneSystem
    {
    public:
        ScriptSystem();
        ~ScriptSystem() override;

        ScriptSystem(const ScriptSystem&) = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;

        std::string_view GetName() const override { return "Scripts"; }
        ScenePhase GetPhase() const override { return ScenePhase::Update; }

        void OnAttach(Scene& scene) override;
        void OnUpdate(Scene& scene, Timestep timestep) override;
        void OnDetach(Scene& scene) override;

        bool IsRunning() const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
