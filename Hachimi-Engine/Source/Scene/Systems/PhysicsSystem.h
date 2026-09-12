#pragma once

#include "Core/Memory.h"
#include "Scene/SceneSystem.h"

namespace HachimiEngine
{
    class PhysicsWorld;
    struct PhysicsSettings;

    // Owns the Box3D world for one running scene and advances it in the fixed update phase.
    //
    // The system stays attached even when the world cannot be created: it then simply does
    // nothing, rather than taking the rest of the scene's simulation down with it.
    class PhysicsSystem final : public SceneSystem
    {
    public:
        std::string_view GetName() const override { return "Physics"; }
        ScenePhase GetPhase() const override { return ScenePhase::FixedUpdate; }

        void OnAttach(Scene& scene) override;
        void OnUpdate(Scene& scene, Timestep timestep) override;
        void OnDetach(Scene& scene) override;

        bool IsRunning() const;
        PhysicsWorld* GetWorld() { return m_World.get(); }
        const PhysicsWorld* GetWorld() const { return m_World.get(); }

    private:
        Scope<PhysicsWorld> m_World;
    };
}
