// Deferred edits operate only on CPU scene data, so ordering and invalidation need no GL context.
#include <doctest/doctest.h>

#include "Scene/Scene.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Components/RelationshipComponent.h"
#include "Scripting/ScriptEntity.h"

using namespace HachimiEngine;

TEST_SUITE("Scripting")
{
    TEST_CASE("pending script handles reject access and resolve at the frame boundary")
    {
        Scene scene;
        auto state = scene.GetScriptCommands().CreateEntity("Created");
        ScriptEntity handle(state, &scene);
        CHECK(handle.IsPending());
        CHECK_FALSE(handle.IsValid());
        CHECK(handle.GetName().empty());
        handle.SetPosition({ 4.0f, 5.0f, 6.0f });
        handle.AddComponent("RigidbodyComponent");
        scene.GetScriptCommands().Flush(scene);
        CHECK_FALSE(handle.IsPending());
        REQUIRE(handle.IsValid());
        CHECK(handle.GetName() == "Created");
        CHECK(handle.GetPosition() == Math::Vec3(0.0f));
        CHECK(handle.Resolve().HasComponent<RigidbodyComponent>());
    }

    TEST_CASE("queued structural edits execute in order and ignore operations after destruction")
    {
        Scene scene;
        auto& queue = scene.GetScriptCommands();
        auto parent = queue.CreateEntity("Parent");
        auto child = queue.CreateEntity("Child");
        queue.SetParent(child, parent);
        queue.AddComponent(child, "RigidbodyComponent");
        queue.Flush(scene);
        REQUIRE(child->Value);
        CHECK(child->Value.GetComponent<RelationshipComponent>().Parent == parent->Value.GetUUID());
        queue.ClearParent(child);
        queue.RemoveComponent(child, "RigidbodyComponent");
        queue.RemoveComponent(child, "TransformComponent");
        queue.Flush(scene);
        CHECK(child->Value.GetComponent<RelationshipComponent>().Parent == UUID::Invalid());
        CHECK_FALSE(child->Value.HasComponent<RigidbodyComponent>());
        CHECK(child->Value.HasComponent<TransformComponent>());
        Entity stale = child->Value;
        queue.DestroyEntity(child);
        queue.AddComponent(child, "RigidbodyComponent");
        queue.SetParent(child, parent);
        queue.Flush(scene);
        CHECK_FALSE(stale);
        CHECK_FALSE(child->Value);
        CHECK(queue.IsEmpty());
    }

    TEST_CASE("a recycled entity slot cannot be mutated through a stale script handle")
    {
        Scene scene;
        auto entity = scene.CreateEntity("Old");
        ScriptEntity handle(entity, &scene);
        scene.DestroyEntity(entity);
        auto replacement = scene.CreateEntity("New");
        handle.SetName("Wrong");
        handle.AddComponent("RigidbodyComponent");
        scene.GetScriptCommands().Flush(scene);
        CHECK(replacement.GetName() == "New");
        CHECK_FALSE(replacement.HasComponent<RigidbodyComponent>());
    }

    TEST_CASE("clearing a queue cancels pending creation")
    {
        Scene scene;
        auto state = scene.GetScriptCommands().CreateEntity("Cancelled");
        scene.GetScriptCommands().Clear();
        scene.GetScriptCommands().Flush(scene);
        CHECK_FALSE(state->Pending);
        CHECK_FALSE(state->Value);
    }
}
