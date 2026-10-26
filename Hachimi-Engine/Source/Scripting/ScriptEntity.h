#pragma once

#include "Scripting/ScriptCommandQueue.h"

namespace HachimiEngine
{
    class PhysicsWorld;

    // Backend-independent script handle. A pending or destroyed entity always reads as empty.
    class ScriptEntity
    {
    public:
        ScriptEntity(Entity entity, Scene* scene);
        ScriptEntity(Ref<ScriptEntityState> state, Scene* scene);
        bool IsPending() const { return m_State->Pending; }
        bool IsValid() const;
        Entity Resolve() const;
        Ref<ScriptEntityState> GetState() const { return m_State; }
        std::string GetName() const;
        void SetName(const std::string& name);
        std::string GetUUID() const;
        Math::Vec3 GetPosition() const;
        void SetPosition(const Math::Vec3& value);
        Math::Vec3 GetRotation() const;
        void SetRotation(const Math::Vec3& value);
        Math::Vec3 GetScale() const;
        void SetScale(const Math::Vec3& value);
        Math::Vec3 GetWorldPosition() const;
        void Translate(const Math::Vec3& value);
        void Rotate(const Math::Vec3& value);
        bool HasComponent(const std::string& name) const;
        void AddComponent(const std::string& name);
        void RemoveComponent(const std::string& name);
        void Destroy();
        void SetParent(const ScriptEntity& parent);
        void ClearParent();
        Math::Vec3 GetLinearVelocity() const;
        void SetLinearVelocity(const Math::Vec3& value);
        Math::Vec3 GetAngularVelocity() const;
        void SetAngularVelocity(const Math::Vec3& value);
        void ApplyForce(const Math::Vec3& value);
        void ApplyImpulse(const Math::Vec3& value);
        void ApplyTorque(const Math::Vec3& value);
        void ApplyAngularImpulse(const Math::Vec3& value);
        void Teleport(const Math::Vec3& position, const Math::Vec3& rotation);
        bool IsTriggered() const;
        void SetTriggered(bool value);

    private:
        PhysicsWorld* GetPhysics() const;
        uint64_t GetKey() const;
        Ref<ScriptEntityState> m_State;
        Scene* m_Scene = nullptr;
        mutable bool m_WarnedPhysics = false;
    };
}
