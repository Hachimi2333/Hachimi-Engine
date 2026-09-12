// ComponentRegistry: the table that every generic component operation goes through.
//
// The point of these cases is that they are written against the registry rather than against a
// list of component types: a newly registered component is covered the moment it is added, so a
// component cannot be half-integrated (created but not cloned, saved but not loaded) without a
// test failing.

#include <doctest/doctest.h>

#include "Scene/ComponentRegistry.h"
#include "Scene/Components/IDComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <yaml-cpp/yaml.h>

#include <string>
#include <unordered_set>

using namespace HachimiEngine;

namespace
{
    // Emits the component through its own serializer, wrapped in the entity map it expects when
    // reading it back.
    YAML::Node EmitEntityNode(const ComponentDescriptor& descriptor, const entt::registry& registry, entt::entity entity)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        descriptor.Serialize(out, registry, entity);
        out << YAML::EndMap;

        return YAML::Load(out.c_str());
    }

    std::string EmitToString(const YAML::Node& node)
    {
        YAML::Emitter out;
        out << node;
        return out.c_str();
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("the built-in components are registered once, with unique keys")
    {
        const std::vector<ComponentDescriptor>& descriptors = ComponentRegistry::GetDescriptors();

        REQUIRE_FALSE(descriptors.empty());

        std::unordered_set<std::string> names;
        std::unordered_set<entt::id_type> typeIds;

        for (const ComponentDescriptor& descriptor : descriptors)
        {
            CHECK(descriptor.IsValid());
            CHECK_FALSE(descriptor.Name.empty());
            CHECK_FALSE(descriptor.DisplayName.empty());
            CHECK(descriptor.TypeID != entt::null);

            CHECK(names.insert(std::string(descriptor.Name)).second);
            CHECK(typeIds.insert(descriptor.TypeID).second);
        }

        // Calling it again must not duplicate the table.
        ComponentRegistry::EnsureBuiltinComponentsRegistered();
        CHECK(ComponentRegistry::GetDescriptors().size() == descriptors.size());
    }

    TEST_CASE("every descriptor can be found by its type id")
    {
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            const ComponentDescriptor* found = ComponentRegistry::Find(descriptor.TypeID);
            REQUIRE(found != nullptr);
            CHECK(found->Name == descriptor.Name);
        }

        CHECK(ComponentRegistry::Find(entt::type_hash<int>::value()) == nullptr);
    }

    TEST_CASE("the scene creates every required component on a new entity")
    {
        Scene scene;
        Entity entity = scene.CreateEntity("Probe");

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.Required)
            {
                CHECK(descriptor.Has(scene.GetRegistry(), entity.GetHandle()));
            }
        }
    }

    TEST_CASE("every descriptor survives add, clone, serialize and deserialize")
    {
        Scene scene;
        Entity source = scene.CreateEntity("Source");

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (!descriptor.Has(scene.GetRegistry(), source.GetHandle()))
            {
                descriptor.AddDefault(scene.GetRegistry(), source.GetHandle());
            }
            REQUIRE(descriptor.Has(scene.GetRegistry(), source.GetHandle()));

            const std::string before = EmitToString(EmitEntityNode(descriptor, scene.GetRegistry(), source.GetHandle()));
            REQUIRE_FALSE(before.empty());

            // Clone into an empty registry: a duplicate target may already carry a required
            // component, which is why Clone has to replace rather than insert.
            entt::registry cloneRegistry;
            const entt::entity cloneEntity = cloneRegistry.create();
            descriptor.Clone(scene.GetRegistry(), source.GetHandle(), cloneRegistry, cloneEntity);
            REQUIRE(descriptor.Has(cloneRegistry, cloneEntity));

            const std::string cloned = EmitToString(EmitEntityNode(descriptor, cloneRegistry, cloneEntity));
            CHECK(cloned == before);

            // Deserializing the emitted block must reproduce it exactly.
            entt::registry loadedRegistry;
            const entt::entity loadedEntity = loadedRegistry.create();
            const YAML::Node node = EmitEntityNode(descriptor, scene.GetRegistry(), source.GetHandle());
            REQUIRE(descriptor.Deserialize(node, loadedRegistry, loadedEntity));
            REQUIRE(descriptor.Has(loadedRegistry, loadedEntity));

            const std::string reloaded = EmitToString(EmitEntityNode(descriptor, loadedRegistry, loadedEntity));
            CHECK(reloaded == before);
        }
    }

    TEST_CASE("a missing key leaves the component alone and reports failure")
    {
        Scene scene;
        Entity entity = scene.CreateEntity("Probe");
        const YAML::Node emptyNode = YAML::Load("{}");

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            const bool wasPresent = descriptor.Has(scene.GetRegistry(), entity.GetHandle());
            CHECK_FALSE(descriptor.Deserialize(emptyNode, scene.GetRegistry(), entity.GetHandle()));
            CHECK(descriptor.Has(scene.GetRegistry(), entity.GetHandle()) == wasPresent);
        }
    }

    TEST_CASE("required components refuse to be removed, optional ones do not")
    {
        Scene scene;
        Entity entity = scene.CreateEntity("Probe");

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.Required)
            {
                descriptor.Remove(scene.GetRegistry(), entity.GetHandle());
                CHECK(descriptor.Has(scene.GetRegistry(), entity.GetHandle()));
                continue;
            }

            if (!descriptor.Has(scene.GetRegistry(), entity.GetHandle()))
            {
                descriptor.AddDefault(scene.GetRegistry(), entity.GetHandle());
            }

            descriptor.Remove(scene.GetRegistry(), entity.GetHandle());
            CHECK_FALSE(descriptor.Has(scene.GetRegistry(), entity.GetHandle()));
        }
    }

    TEST_CASE("every add-component preset creates its component")
    {
        Scene scene;

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            for (const ComponentAddPreset& preset : descriptor.Presets)
            {
                REQUIRE(preset.Add != nullptr);
                CHECK_FALSE(preset.Label.empty());

                // A fresh entity, because a preset is only offered for a component the entity
                // does not have yet.
                Entity entity = scene.CreateEntity("Preset Probe");
                preset.Add(scene.GetRegistry(), entity.GetHandle());
                CHECK(descriptor.Has(scene.GetRegistry(), entity.GetHandle()));
            }
        }
    }

    TEST_CASE("duplicating an entity keeps only the components that belong to the copy")
    {
        Scene scene;
        Entity source = scene.CreateEntity("Source");

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (!descriptor.Has(scene.GetRegistry(), source.GetHandle()))
            {
                descriptor.AddDefault(scene.GetRegistry(), source.GetHandle());
            }
        }

        Entity duplicate = scene.DuplicateEntity(source);
        REQUIRE(static_cast<bool>(duplicate));

        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetDescriptors())
        {
            if (descriptor.CopiedOnDuplicate)
            {
                CHECK(descriptor.Has(scene.GetRegistry(), duplicate.GetHandle()));
            }
            else
            {
                // The copy has its own identity and no place in the hierarchy yet.
                CHECK(descriptor.Has(scene.GetRegistry(), duplicate.GetHandle()));
                if (descriptor.TypeID == entt::type_hash<IDComponent>::value())
                {
                    CHECK(duplicate.GetUUID() != source.GetUUID());
                }
            }
        }

        CHECK(duplicate.GetName() == "Source Copy");
    }
}
