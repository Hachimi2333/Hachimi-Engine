#include "Scripting/ScriptEntity.h"

#include "Core/Log.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"

namespace HachimiEngine
{
    ScriptEntity::ScriptEntity(Entity entity, Scene* scene)
        : m_State(CreateRef<ScriptEntityState>()), m_Scene(scene)
    {
        m_State->Value = entity;
    }

    ScriptEntity::ScriptEntity(Ref<ScriptEntityState> state, Scene* scene)
        : m_State(std::move(state)), m_Scene(scene)
    {
    }

    Entity ScriptEntity::Resolve() const
    {
        if (m_State->Pending)
        {
            if (!m_State->Warned)
            {
                HE_CORE_WARN("Script accessed a pending entity before the command queue was flushed");
                m_State->Warned = true;
            }
            return {};
        }
        return m_State->Value && m_State->Value.GetScene() == m_Scene ? m_State->Value : Entity {};
    }

    bool ScriptEntity::IsValid() const { return !m_State->Pending && static_cast<bool>(m_State->Value); }
    std::string ScriptEntity::GetName() const { auto e = Resolve(); return e ? e.GetName() : std::string(); }
    void ScriptEntity::SetName(const std::string& name) { auto e = Resolve(); if (e) e.GetComponent<TagComponent>().Tag = name; }
    std::string ScriptEntity::GetUUID() const { auto e = Resolve(); return e ? e.GetUUID().ToString() : std::string(); }
    Math::Vec3 ScriptEntity::GetPosition() const { auto e = Resolve(); return e ? e.Transform().Position : Math::Vec3(0.0f); }
    void ScriptEntity::SetPosition(const Math::Vec3& value) { auto e = Resolve(); if (e) e.Transform().Position = value; }
    Math::Vec3 ScriptEntity::GetRotation() const { auto e = Resolve(); return e ? e.Transform().Rotation : Math::Vec3(0.0f); }
    void ScriptEntity::SetRotation(const Math::Vec3& value) { auto e = Resolve(); if (e) e.Transform().Rotation = value; }
    Math::Vec3 ScriptEntity::GetScale() const { auto e = Resolve(); return e ? e.Transform().Scale : Math::Vec3(1.0f); }
    void ScriptEntity::SetScale(const Math::Vec3& value) { auto e = Resolve(); if (e) e.Transform().Scale = value; }
    Math::Vec3 ScriptEntity::GetWorldPosition() const
    {
        auto e = Resolve();
        return e ? Math::Vec3(m_Scene->GetWorldTransform(e.GetHandle())[3]) : Math::Vec3(0.0f);
    }
    void ScriptEntity::Translate(const Math::Vec3& value) { auto e = Resolve(); if (e) e.Transform().Position += value; }
    void ScriptEntity::Rotate(const Math::Vec3& value) { auto e = Resolve(); if (e) e.Transform().Rotation += value; }

    bool ScriptEntity::HasComponent(const std::string& name) const
    {
        auto e = Resolve();
        if (!e) return false;
        for (const auto& descriptor : ComponentRegistry::GetDescriptors())
            if (descriptor.Name == name || descriptor.DisplayName == name)
                return descriptor.Has(e.GetRegistry(), e.GetHandle());
        return false;
    }

    void ScriptEntity::AddComponent(const std::string& name) { m_Scene->GetScriptCommands().AddComponent(m_State, name); }
    void ScriptEntity::RemoveComponent(const std::string& name) { m_Scene->GetScriptCommands().RemoveComponent(m_State, name); }
    void ScriptEntity::Destroy() { m_Scene->GetScriptCommands().DestroyEntity(m_State); }
    void ScriptEntity::SetParent(const ScriptEntity& parent) { m_Scene->GetScriptCommands().SetParent(m_State, parent.m_State); }
    void ScriptEntity::ClearParent() { m_Scene->GetScriptCommands().ClearParent(m_State); }

    uint64_t ScriptEntity::GetKey() const { auto e = Resolve(); return e ? static_cast<uint64_t>(e.GetHandle()) : 0; }
    PhysicsWorld* ScriptEntity::GetPhysics() const
    {
        if (!Resolve()) return nullptr;
        auto* system = m_Scene->FindSystem<PhysicsSystem>();
        if (system != nullptr && system->GetWorld() != nullptr) return system->GetWorld();
        if (!m_WarnedPhysics)
        {
            HE_CORE_WARN("Script requested physics in a scene without a physics world");
            m_WarnedPhysics = true;
        }
        return nullptr;
    }

    Math::Vec3 ScriptEntity::GetLinearVelocity() const { auto* w = GetPhysics(); return w ? w->GetLinearVelocity(GetKey()) : Math::Vec3(0.0f); }
    void ScriptEntity::SetLinearVelocity(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->SetLinearVelocity(GetKey(), value); }
    Math::Vec3 ScriptEntity::GetAngularVelocity() const { auto* w = GetPhysics(); return w ? w->GetAngularVelocity(GetKey()) : Math::Vec3(0.0f); }
    void ScriptEntity::SetAngularVelocity(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->SetAngularVelocity(GetKey(), value); }
    void ScriptEntity::ApplyForce(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->ApplyForce(GetKey(), value); }
    void ScriptEntity::ApplyImpulse(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->ApplyImpulse(GetKey(), value); }
    void ScriptEntity::ApplyTorque(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->ApplyTorque(GetKey(), value); }
    void ScriptEntity::ApplyAngularImpulse(const Math::Vec3& value) { if (auto* w = GetPhysics()) w->ApplyAngularImpulse(GetKey(), value); }
    void ScriptEntity::Teleport(const Math::Vec3& position, const Math::Vec3& rotation)
    {
        if (auto* w = GetPhysics()) w->Teleport(GetKey(), position, Math::Quat(Math::Radians(rotation)), *m_Scene);
    }
    bool ScriptEntity::IsTriggered() const { auto e = Resolve(); return e && e.HasComponent<ColliderComponent>() && e.GetComponent<ColliderComponent>().IsTrigger; }
    void ScriptEntity::SetTriggered(bool value) { auto e = Resolve(); if (e && e.HasComponent<ColliderComponent>()) e.GetComponent<ColliderComponent>().IsTrigger = value; }
}
