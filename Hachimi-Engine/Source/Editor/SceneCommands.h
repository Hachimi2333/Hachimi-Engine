#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Base.h"
#include "Core/Memory.h"
#include "Editor/EditorCommand.h"
#include "Math/Math.h"

#include <entt/entt.hpp>

#include <string>

namespace HachimiEngine
{
    class Entity;
    class Scene;

    // Commands the editor issues while editing a scene.
    //
    // Every factory captures the state it is about to change, so the caller's pattern is always
    // "capture, then act": the command already knows the value to restore and reverting never has
    // to reconstruct it from the scene. A factory returns nullptr when the entity cannot carry the
    // edit, which the caller can push straight through, because the history ignores null commands.
    namespace SceneCommands
    {
        // Transform edits, merged so one gizmo drag is one undo step.
        Scope<EditorCommand> MakeSetTransform(Entity entity, const Math::Vec3& position,
                                              const Math::Vec3& rotation, const Math::Vec3& scale);

        // Entity name, merged while the user is still typing.
        Scope<EditorCommand> MakeSetTag(Entity entity, const std::string& before, const std::string& after);

        // The mesh renderer's inline scalars: what the entity looks like with no material asset
        // assigned, and what it falls back to for the channels a material does not set.
        Scope<EditorCommand> MakeSetMeshColor(Entity entity, const Math::Vec4& color);
        Scope<EditorCommand> MakeSetMeshRoughness(Entity entity, float roughness);
        Scope<EditorCommand> MakeSetMeshMetallic(Entity entity, float metallic);
        Scope<EditorCommand> MakeSetPrimitive(Entity entity, int primitiveIndex);

        // Light and camera scalars, which the inspector edits one slider at a time.
        Scope<EditorCommand> MakeSetLightIntensity(Entity entity, float intensity);
        Scope<EditorCommand> MakeSetLightColor(Entity entity, const Math::Vec3& color);
        Scope<EditorCommand> MakeSetLightRange(Entity entity, float range);
        Scope<EditorCommand> MakeSetCameraFieldOfView(Entity entity, float fieldOfView);
        Scope<EditorCommand> MakeSetCameraNearClip(Entity entity, float nearClip);
        Scope<EditorCommand> MakeSetCameraFarClip(Entity entity, float farClip);

        // Collider and rigidbody scalars, which the physics drawer edits.
        Scope<EditorCommand> MakeSetColliderFriction(Entity entity, float friction);
        Scope<EditorCommand> MakeSetColliderRestitution(Entity entity, float restitution);
        Scope<EditorCommand> MakeSetColliderRadius(Entity entity, float radius);
        Scope<EditorCommand> MakeSetRigidbodyLinearDamping(Entity entity, float damping);
        Scope<EditorCommand> MakeSetRigidbodyAngularDamping(Entity entity, float damping);

        // Asset references.
        Scope<EditorCommand> MakeSetMeshMaterial(Entity entity, AssetHandle material);
        Scope<EditorCommand> MakeSetScriptReference(Entity entity, uint32_t slotIndex, AssetHandle script,
                                                    const std::string& displayName);

        // Attaching and detaching components. Both go through ComponentRegistry, so a newly
        // registered component is undoable without a change here.
        Scope<EditorCommand> MakeAddComponent(Entity entity, entt::id_type typeId);
        // Removing a component loses its values, so the whole entity is snapshotted: reverting
        // restores what was there rather than a fresh default.
        Scope<EditorCommand> MakeRemoveComponent(Scene& scene, Entity entity, entt::id_type typeId);

        // Entity lifetime. The whole subtree is recorded, because destroying a parent takes its
        // children with it and undoing that has to bring them back with their components intact.
        Scope<EditorCommand> MakeCreateEntity(Scene& scene, Entity entity);
        Scope<EditorCommand> MakeDestroyEntity(Scene& scene, Entity entity);
        // Copies an entity and returns a command that undoes the copy. outDuplicate receives the
        // new entity so the caller can select it.
        Scope<EditorCommand> MakeDuplicateEntity(Scene& scene, Entity entity, Entity& outDuplicate);
    }
}
