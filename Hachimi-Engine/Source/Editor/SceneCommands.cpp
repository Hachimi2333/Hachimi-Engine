#include "Editor/SceneCommands.h"

#include "Asset/AssetHandle.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Renderer/MeshFactory.h"
#include "Serialization/SceneSerializer.h"
#include "Math/Math.h"

#include <utility>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        // Every command reaches its entity through its UUID rather than a handle: a handle is an
        // index into a registry that undo and redo can rearrange, while a UUID is the identity the
        // scene itself uses.
        Entity FindEntity(Scene& scene, UUID id)
        {
            return scene.GetEntityByUUID(id);
        }

        void CollectSubtree(Scene& scene, Entity entity, std::vector<Entity>& outEntities)
        {
            if (!entity)
            {
                return;
            }

            outEntities.push_back(entity);
            for (const Entity child : scene.GetChildren(entity))
            {
                CollectSubtree(scene, child, outEntities);
            }
        }

        std::string SnapshotSubtree(Scene& scene, Entity entity)
        {
            std::vector<Entity> entities;
            CollectSubtree(scene, entity, entities);
            return SceneSerializer::SerializeEntitiesToString(scene, entities);
        }

        // Base for the "one value on one entity" commands.
        //
        // They all share the same shape - reach the entity, read the old value when the command is
        // built, write a value on apply and the other on revert, and fold a repeated edit of the
        // same field into themselves - so only the field access differs. Deriving from this keeps a
        // new scalar property down to a factory function instead of a command class.
        template<typename TValue, typename TComponent>
        class FieldCommand : public EditorCommand
        {
        public:
            using Member = TValue TComponent::*;

            FieldCommand(std::string name, UUID entityId, Member field, const TValue& before, const TValue& after)
                : m_Name(std::move(name))
                , m_EntityId(entityId)
                , m_Field(field)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return m_Name; }

            void Apply(Scene& scene) override { Write(scene, m_After); }
            void Revert(Scene& scene) override { Write(scene, m_Before); }

            bool TryMerge(const EditorCommand& next) override
            {
                const auto* other = dynamic_cast<const FieldCommand*>(&next);
                if (other == nullptr || other->m_EntityId != m_EntityId || other->m_Field != m_Field)
                {
                    return false;
                }

                m_After = other->m_After;
                return true;
            }

        private:
            void Write(Scene& scene, const TValue& value)
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (entity && entity.HasComponent<TComponent>())
                {
                    entity.GetComponent<TComponent>().*m_Field = value;
                }
            }

            std::string m_Name;
            UUID m_EntityId;
            Member m_Field;
            TValue m_Before;
            TValue m_After;
        };

        template<typename TValue, typename TComponent>
        Scope<EditorCommand> MakeFieldCommand(Entity entity, const char* name, TValue TComponent::* field,
                                              const TValue& after)
        {
            if (!entity || !entity.HasComponent<TComponent>())
            {
                return nullptr;
            }

            const TValue before = entity.GetComponent<TComponent>().*field;
            if (before == after)
            {
                return nullptr;
            }

            return CreateScope<FieldCommand<TValue, TComponent>>(name, entity.GetUUID(), field, before, after);
        }

        // Both states are captured as plain values by the factory, so the command never holds a
        // reference into the registry and cannot be invalidated by a later edit.
        class SetTransformCommand final : public EditorCommand
        {
        public:
            struct Values
            {
                Math::Vec3 Position;
                Math::Vec3 Rotation;
                Math::Vec3 Scale;
            };

            SetTransformCommand(UUID entityId, const Values& before, const Values& after)
                : m_EntityId(entityId)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return "Move Entity"; }

            void Apply(Scene& scene) override
            {
                if (TransformComponent* transform = FindTransform(scene))
                {
                    Write(*transform, m_After);
                }
            }

            void Revert(Scene& scene) override
            {
                if (TransformComponent* transform = FindTransform(scene))
                {
                    Write(*transform, m_Before);
                }
            }

            // A gizmo reports its result every frame, so the entry is created once and then widened
            // to the latest value; without this a drag would cost one undo step per frame.
            bool TryMerge(const EditorCommand& next) override
            {
                const auto* other = dynamic_cast<const SetTransformCommand*>(&next);
                if (other == nullptr || other->m_EntityId != m_EntityId)
                {
                    return false;
                }

                m_After = other->m_After;
                return true;
            }

        private:
            static void Write(TransformComponent& transform, const Values& values)
            {
                transform.Position = values.Position;
                transform.Rotation = values.Rotation;
                transform.Scale = values.Scale;
            }

            TransformComponent* FindTransform(Scene& scene) const
            {
                Entity entity = FindEntity(scene, m_EntityId);
                return entity && entity.HasComponent<TransformComponent>()
                    ? &entity.GetComponent<TransformComponent>()
                    : nullptr;
            }

            UUID m_EntityId;
            Values m_Before;
            Values m_After;
        };

        class SetTagCommand final : public EditorCommand
        {
        public:
            SetTagCommand(UUID entityId, std::string before, std::string after)
                : m_EntityId(entityId)
                , m_Before(std::move(before))
                , m_After(std::move(after))
            {
            }

            std::string_view GetName() const override { return "Rename Entity"; }

            void Apply(Scene& scene) override { Write(scene, m_After); }
            void Revert(Scene& scene) override { Write(scene, m_Before); }

            bool TryMerge(const EditorCommand& next) override
            {
                const auto* other = dynamic_cast<const SetTagCommand*>(&next);
                if (other == nullptr || other->m_EntityId != m_EntityId)
                {
                    return false;
                }

                m_After = other->m_After;
                return true;
            }

        private:
            void Write(Scene& scene, const std::string& value)
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (entity && entity.HasComponent<TagComponent>())
                {
                    entity.GetComponent<TagComponent>().Tag = value;
                }
            }

            UUID m_EntityId;
            std::string m_Before;
            std::string m_After;
        };

        // Changing the primitive replaces the geometry, so the command owns both the enum and the
        // CPU mesh it selects.
        class SetPrimitiveCommand final : public EditorCommand
        {
        public:
            SetPrimitiveCommand(UUID entityId, PrimitiveMeshType before, PrimitiveMeshType after)
                : m_EntityId(entityId)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return "Change Primitive"; }

            void Apply(Scene& scene) override { Write(scene, m_After); }
            void Revert(Scene& scene) override { Write(scene, m_Before); }

        private:
            void Write(Scene& scene, PrimitiveMeshType primitive)
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (!entity || !entity.HasComponent<MeshRendererComponent>())
                {
                    return;
                }

                // Through the component, so the enum and the geometry it names are always replaced
                // together and undo restores both.
                entity.GetComponent<MeshRendererComponent>().SetPrimitive(primitive);
            }

            UUID m_EntityId;
            PrimitiveMeshType m_Before = PrimitiveMeshType::Cube;
            PrimitiveMeshType m_After = PrimitiveMeshType::Cube;
        };

        class SetMeshMaterialCommand final : public EditorCommand
        {
        public:
            SetMeshMaterialCommand(UUID entityId, AssetHandle before, AssetHandle after)
                : m_EntityId(entityId)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return "Assign Material"; }

            void Apply(Scene& scene) override { Write(scene, m_After); }
            void Revert(Scene& scene) override { Write(scene, m_Before); }

        private:
            void Write(Scene& scene, AssetHandle handle)
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (entity && entity.HasComponent<MeshRendererComponent>())
                {
                    entity.GetComponent<MeshRendererComponent>().Material = handle;
                }
            }

            UUID m_EntityId;
            AssetHandle m_Before;
            AssetHandle m_After;
        };

        class SetScriptReferenceCommand final : public EditorCommand
        {
        public:
            SetScriptReferenceCommand(UUID entityId, uint32_t slotIndex, AssetHandle before, std::string beforeName,
                                      AssetHandle after, std::string afterName)
                : m_EntityId(entityId)
                , m_SlotIndex(slotIndex)
                , m_Before(before)
                , m_BeforeName(std::move(beforeName))
                , m_After(after)
                , m_AfterName(std::move(afterName))
            {
            }

            std::string_view GetName() const override { return "Assign Script"; }

            void Apply(Scene& scene) override { Write(scene, m_After, m_AfterName); }
            void Revert(Scene& scene) override { Write(scene, m_Before, m_BeforeName); }

        private:
            void Write(Scene& scene, AssetHandle handle, const std::string& displayName)
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (!entity || !entity.HasComponent<ScriptComponent>())
                {
                    return;
                }

                auto& scripts = entity.GetComponent<ScriptComponent>().Scripts;
                if (m_SlotIndex < scripts.size())
                {
                    scripts[m_SlotIndex].Script = handle;
                    scripts[m_SlotIndex].DisplayName = displayName;
                }
            }

            UUID m_EntityId;
            uint32_t m_SlotIndex = 0;
            AssetHandle m_Before;
            std::string m_BeforeName;
            AssetHandle m_After;
            std::string m_AfterName;
        };

        class AddComponentCommand final : public EditorCommand
        {
        public:
            AddComponentCommand(UUID entityId, const ComponentDescriptor& descriptor)
                : m_EntityId(entityId)
                , m_Descriptor(&descriptor)
            {
            }

            std::string_view GetName() const override { return "Add Component"; }

            void Apply(Scene& scene) override
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (entity && m_Descriptor->AddDefault != nullptr)
                {
                    m_Descriptor->AddDefault(entity.GetRegistry(), entity.GetHandle());
                }
            }

            void Revert(Scene& scene) override
            {
                Entity entity = FindEntity(scene, m_EntityId);
                if (entity && m_Descriptor->Remove != nullptr)
                {
                    m_Descriptor->Remove(entity.GetRegistry(), entity.GetHandle());
                }
            }

        private:
            UUID m_EntityId;
            const ComponentDescriptor* m_Descriptor = nullptr;
        };

        class RemoveComponentCommand final : public EditorCommand
        {
        public:
            RemoveComponentCommand(UUID entityId, const ComponentDescriptor& descriptor, std::string snapshot)
                : m_EntityId(entityId)
                , m_Descriptor(&descriptor)
                , m_Snapshot(std::move(snapshot))
            {
            }

            std::string_view GetName() const override { return "Remove Component"; }

            void Apply(Scene& scene) override
            {
                const Entity entity = FindEntity(scene, m_EntityId);
                if (entity && m_Descriptor->Remove != nullptr)
                {
                    m_Descriptor->Remove(entity.GetRegistry(), entity.GetHandle());
                }
            }

            void Revert(Scene& scene) override
            {
                const Entity entity = FindEntity(scene, m_EntityId);
                if (!entity)
                {
                    return;
                }

                // The component is put back through its own descriptor, so every field returns -
                // including ones the editor never shows - and the entity keeps its identity,
                // hierarchy and other components.
                SceneSerializer::DeserializeComponentFromString(scene, entity, m_Snapshot);
            }

        private:
            UUID m_EntityId;
            const ComponentDescriptor* m_Descriptor = nullptr;
            std::string m_Snapshot;
        };

        // Creating and destroying an entity are the same problem from both sides: an entity is a
        // subtree of components, and its serialized form is the only complete record of it.
        class EntityLifetimeCommand final : public EditorCommand
        {
        public:
            EntityLifetimeCommand(UUID entityId, std::string snapshot, bool existsAfter)
                : m_EntityId(entityId)
                , m_Snapshot(std::move(snapshot))
                , m_ExistsAfter(existsAfter)
            {
            }

            std::string_view GetName() const override
            {
                return m_ExistsAfter ? "Create Entity" : "Delete Entity";
            }

            void Apply(Scene& scene) override
            {
                if (m_ExistsAfter)
                {
                    Restore(scene);
                }
                else
                {
                    Remove(scene);
                }
            }

            void Revert(Scene& scene) override
            {
                if (m_ExistsAfter)
                {
                    Remove(scene);
                }
                else
                {
                    Restore(scene);
                }
            }

            // Refreshes the record while the entity is still alive, so a create pushed after the
            // user finished editing stores the edited state rather than the empty one it started as.
            void Commit(Scene& scene) override
            {
                const Entity entity = FindEntity(scene, m_EntityId);
                if (entity)
                {
                    m_Snapshot = SnapshotSubtree(scene, entity);
                }
            }

        private:
            void Restore(Scene& scene)
            {
                if (FindEntity(scene, m_EntityId))
                {
                    return;
                }

                SceneSerializer::DeserializeEntitiesFromString(scene, m_Snapshot);
            }

            void Remove(Scene& scene)
            {
                const Entity entity = FindEntity(scene, m_EntityId);
                if (entity)
                {
                    scene.DestroyEntity(entity);
                }
            }

            UUID m_EntityId;
            std::string m_Snapshot;
            bool m_ExistsAfter = true;
        };
    }

    Scope<EditorCommand> SceneCommands::MakeSetTransform(Entity entity, const Math::Vec3& position,
                                                         const Math::Vec3& rotation, const Math::Vec3& scale)
    {
        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            return nullptr;
        }

        const TransformComponent& current = entity.GetComponent<TransformComponent>();

        // Both states are captured as plain values here, by the factory that knows the entity, so
        // the command never reads the registry to find out what it replaced.
        SetTransformCommand::Values before;
        before.Position = current.Position;
        before.Rotation = current.Rotation;
        before.Scale = current.Scale;

        SetTransformCommand::Values after;
        after.Position = position;
        after.Rotation = rotation;
        after.Scale = scale;

        return CreateScope<SetTransformCommand>(entity.GetUUID(), before, after);
    }

    Scope<EditorCommand> SceneCommands::MakeSetTag(Entity entity, const std::string& before, const std::string& after)
    {
        if (!entity || !entity.HasComponent<TagComponent>() || before == after)
        {
            return nullptr;
        }

        return CreateScope<SetTagCommand>(entity.GetUUID(), before, after);
    }

    Scope<EditorCommand> SceneCommands::MakeSetMeshColor(Entity entity, const Math::Vec4& color)
    {
        return MakeFieldCommand<Math::Vec4, MeshRendererComponent>(entity, "Albedo Color",
            &MeshRendererComponent::AlbedoColor, color);
    }

    Scope<EditorCommand> SceneCommands::MakeSetMeshRoughness(Entity entity, float roughness)
    {
        return MakeFieldCommand<float, MeshRendererComponent>(entity, "Roughness",
            &MeshRendererComponent::Roughness, roughness);
    }

    Scope<EditorCommand> SceneCommands::MakeSetMeshMetallic(Entity entity, float metallic)
    {
        return MakeFieldCommand<float, MeshRendererComponent>(entity, "Metallic",
            &MeshRendererComponent::Metallic, metallic);
    }

    Scope<EditorCommand> SceneCommands::MakeSetPrimitive(Entity entity, int primitiveIndex)
    {
        if (!entity || !entity.HasComponent<MeshRendererComponent>())
        {
            return nullptr;
        }

        constexpr int FirstPrimitive = static_cast<int>(PrimitiveMeshType::Cube);
        constexpr int LastPrimitive = static_cast<int>(PrimitiveMeshType::Grid);
        if (primitiveIndex < FirstPrimitive || primitiveIndex > LastPrimitive)
        {
            return nullptr;
        }

        const auto primitive = static_cast<PrimitiveMeshType>(primitiveIndex);
        const PrimitiveMeshType before = entity.GetComponent<MeshRendererComponent>().Primitive;
        if (before == primitive)
        {
            return nullptr;
        }

        return CreateScope<SetPrimitiveCommand>(entity.GetUUID(), before, primitive);
    }

    Scope<EditorCommand> SceneCommands::MakeSetLightIntensity(Entity entity, float intensity)
    {
        return MakeFieldCommand<float, LightComponent>(entity, "Intensity", &LightComponent::Intensity, intensity);
    }

    Scope<EditorCommand> SceneCommands::MakeSetLightColor(Entity entity, const Math::Vec3& color)
    {
        return MakeFieldCommand<Math::Vec3, LightComponent>(entity, "Light Color", &LightComponent::Color, color);
    }

    Scope<EditorCommand> SceneCommands::MakeSetLightRange(Entity entity, float range)
    {
        return MakeFieldCommand<float, LightComponent>(entity, "Light Range", &LightComponent::Range, range);
    }

    Scope<EditorCommand> SceneCommands::MakeSetCameraFieldOfView(Entity entity, float fieldOfView)
    {
        return MakeFieldCommand<float, CameraComponent>(entity, "Field Of View", &CameraComponent::FieldOfView,
            fieldOfView);
    }

    Scope<EditorCommand> SceneCommands::MakeSetCameraNearClip(Entity entity, float nearClip)
    {
        return MakeFieldCommand<float, CameraComponent>(entity, "Near Clip", &CameraComponent::NearClip, nearClip);
    }

    Scope<EditorCommand> SceneCommands::MakeSetCameraFarClip(Entity entity, float farClip)
    {
        return MakeFieldCommand<float, CameraComponent>(entity, "Far Clip", &CameraComponent::FarClip, farClip);
    }

    Scope<EditorCommand> SceneCommands::MakeSetColliderFriction(Entity entity, float friction)
    {
        return MakeFieldCommand<float, ColliderComponent>(entity, "Friction", &ColliderComponent::Friction, friction);
    }

    Scope<EditorCommand> SceneCommands::MakeSetColliderRestitution(Entity entity, float restitution)
    {
        return MakeFieldCommand<float, ColliderComponent>(entity, "Restitution", &ColliderComponent::Restitution,
            restitution);
    }

    Scope<EditorCommand> SceneCommands::MakeSetColliderRadius(Entity entity, float radius)
    {
        return MakeFieldCommand<float, ColliderComponent>(entity, "Collider Radius", &ColliderComponent::Radius,
            radius);
    }

    Scope<EditorCommand> SceneCommands::MakeSetRigidbodyLinearDamping(Entity entity, float damping)
    {
        return MakeFieldCommand<float, RigidbodyComponent>(entity, "Linear Damping",
            &RigidbodyComponent::LinearDamping, damping);
    }

    Scope<EditorCommand> SceneCommands::MakeSetRigidbodyAngularDamping(Entity entity, float damping)
    {
        return MakeFieldCommand<float, RigidbodyComponent>(entity, "Angular Damping",
            &RigidbodyComponent::AngularDamping, damping);
    }

    Scope<EditorCommand> SceneCommands::MakeSetMeshMaterial(Entity entity, AssetHandle material)
    {
        if (!entity || !entity.HasComponent<MeshRendererComponent>())
        {
            return nullptr;
        }

        const AssetHandle before = entity.GetComponent<MeshRendererComponent>().Material;
        if (before == material)
        {
            return nullptr;
        }

        return CreateScope<SetMeshMaterialCommand>(entity.GetUUID(), before, material);
    }

    Scope<EditorCommand> SceneCommands::MakeSetScriptReference(Entity entity, uint32_t slotIndex, AssetHandle script,
                                                               const std::string& displayName)
    {
        if (!entity || !entity.HasComponent<ScriptComponent>())
        {
            return nullptr;
        }

        const auto& scripts = entity.GetComponent<ScriptComponent>().Scripts;
        if (slotIndex >= scripts.size())
        {
            return nullptr;
        }

        return CreateScope<SetScriptReferenceCommand>(entity.GetUUID(), slotIndex, scripts[slotIndex].Script,
            scripts[slotIndex].DisplayName, script, displayName);
    }

    Scope<EditorCommand> SceneCommands::MakeAddComponent(Entity entity, entt::id_type typeId)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(typeId);
        if (!entity || descriptor == nullptr || descriptor->Has(entity.GetRegistry(), entity.GetHandle()))
        {
            return nullptr;
        }

        return CreateScope<AddComponentCommand>(entity.GetUUID(), *descriptor);
    }

    Scope<EditorCommand> SceneCommands::MakeRemoveComponent(Scene& scene, Entity entity, entt::id_type typeId)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::Find(typeId);
        if (!entity || descriptor == nullptr || descriptor->Required)
        {
            return nullptr;
        }

        if (!descriptor->Has(entity.GetRegistry(), entity.GetHandle()))
        {
            return nullptr;
        }

        // Only this component is recorded; reverting puts it back without disturbing the entity.
        return CreateScope<RemoveComponentCommand>(entity.GetUUID(), *descriptor,
            SceneSerializer::SerializeComponentToString(scene, entity, typeId));
    }

    Scope<EditorCommand> SceneCommands::MakeCreateEntity(Scene& scene, Entity entity)
    {
        if (!entity)
        {
            return nullptr;
        }

        return CreateScope<EntityLifetimeCommand>(entity.GetUUID(), SnapshotSubtree(scene, entity), true);
    }

    Scope<EditorCommand> SceneCommands::MakeDestroyEntity(Scene& scene, Entity entity)
    {
        if (!entity)
        {
            return nullptr;
        }

        // The subtree is recorded before the delete, because afterwards there is nothing left to
        // read: destroying a parent takes its children with it. The delete itself happens when the
        // history executes the command, so an unrecorded command changes nothing.
        return CreateScope<EntityLifetimeCommand>(entity.GetUUID(), SnapshotSubtree(scene, entity), false);
    }

    Scope<EditorCommand> SceneCommands::MakeDuplicateEntity(Scene& scene, Entity entity, Entity& outDuplicate)
    {
        outDuplicate = {};
        if (!entity)
        {
            return nullptr;
        }

        // Duplicate first: the command records the copy that now exists, so undo removes it and redo
        // brings the same entity back rather than producing a second copy.
        outDuplicate = scene.DuplicateEntity(entity);
        if (!outDuplicate)
        {
            return nullptr;
        }

        return CreateScope<EntityLifetimeCommand>(outDuplicate.GetUUID(), SnapshotSubtree(scene, outDuplicate), true);
    }
}
