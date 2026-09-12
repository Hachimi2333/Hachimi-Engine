#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Timestep.h"
#include "Physics/PhysicsSettings.h"
#include "Math/Math.h"

#include <cstdint>

namespace HachimiEngine
{
    class Scene;

    // Wraps a Box3D world and keeps the EnTT entity -> b3BodyId mapping.
    // Box3D is a C17 library, so its types are hidden behind an Impl to keep
    // client/editor code independent from the third-party headers.
    class PhysicsWorld
    {
    public:
        explicit PhysicsWorld(const PhysicsSettings& settings);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Creates Box3D bodies for every entity that currently has a
        // RigidbodyComponent, a ColliderComponent, and a TransformComponent.
        void CreateBodies(Scene& scene);

        // Synchronizes ECS changes with Box3D, advances the fixed-step simulation,
        // and writes dynamic body transforms back to the ECS.
        void Update(Scene& scene, Timestep timestep);

        // Destroys the body associated with an entity, if one exists.
        void DestroyBody(uint64_t entityKey);

        bool IsRunning() const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
