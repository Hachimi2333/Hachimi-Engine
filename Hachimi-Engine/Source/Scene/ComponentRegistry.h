#pragma once

#include "Core/Base.h"

#include <entt/entt.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace YAML
{
    class Emitter;
    class Node;
}

namespace HachimiEngine
{
    // Extra "Add Component" entry that creates a component with values other than the
    // defaults, such as one entry per collider shape.
    struct ComponentAddPreset
    {
        std::string_view Label;
        void (*Add)(entt::registry& registry, entt::entity entity) = nullptr;
    };

    // One component type as the generic machinery sees it.
    //
    // Everything the engine does to components in bulk - create them with an entity, duplicate
    // them, save them, load them - goes through this table. A new component is therefore
    // registered once instead of being added to five separate if-chains that can silently fall
    // out of sync: a component missing from Scene::Clone used to compile fine and disappear at
    // runtime.
    struct ComponentDescriptor
    {
        // Stable key inside a .hscene file. Renaming it breaks existing scenes, so treat it as
        // a format identifier rather than a display label.
        std::string_view Name;
        // Label the editor shows.
        std::string_view DisplayName;
        entt::id_type TypeID = entt::null;
        // Required components exist on every entity and cannot be removed. Transform is one of
        // them: rendering and physics both assume it is there.
        bool Required = false;
        // False for components that carry entity identity or hierarchy placement, which a
        // duplicate must not inherit.
        bool CopiedOnDuplicate = true;

        void (*AddDefault)(entt::registry& registry, entt::entity entity) = nullptr;
        void (*Remove)(entt::registry& registry, entt::entity entity) = nullptr;
        bool (*Has)(const entt::registry& registry, entt::entity entity) = nullptr;
        // Copies the component from one entity to another, doing nothing when the source does
        // not have it.
        void (*Clone)(const entt::registry& source, entt::entity sourceEntity,
                      entt::registry& target, entt::entity targetEntity) = nullptr;
        // Writes this component's own key(s) into the entity map currently being emitted.
        void (*Serialize)(YAML::Emitter& out, const entt::registry& registry, entt::entity entity) = nullptr;
        // Reads this component's key from the entity map. Returns false, and changes nothing,
        // when the key is absent or unusable.
        bool (*Deserialize)(const YAML::Node& entityNode, entt::registry& registry, entt::entity entity) = nullptr;

        // Additional "Add Component" menu entries beyond the default one.
        std::span<const ComponentAddPreset> Presets;

        bool IsValid() const
        {
            return AddDefault != nullptr && Remove != nullptr && Has != nullptr && Clone != nullptr
                && Serialize != nullptr && Deserialize != nullptr;
        }
    };

    // Ordered table of every component type the engine knows about.
    //
    // Registration order is the order used for serialization, duplication and the editor's
    // component list. The built-in components register themselves the first time a Scene is
    // constructed, so no executable has to remember a bootstrap call.
    class ComponentRegistry
    {
    public:
        // Must run before the first Scene is constructed: a scene created earlier would not
        // see the new component in any of the bulk operations.
        static void Register(const ComponentDescriptor& descriptor);

        static const std::vector<ComponentDescriptor>& GetDescriptors();
        // Returns nullptr for a component type that was never registered.
        static const ComponentDescriptor* Find(entt::id_type typeId);

        // Registers the built-in components exactly once. The Scene constructor calls it.
        static void EnsureBuiltinComponentsRegistered();

    private:
        static void RegisterBuiltinComponents();
    };
}
