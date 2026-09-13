// CommandHistory and the scene commands: undo/redo without a window.
//
// This is the reason the commands live in the engine rather than in the editor executable: an
// undo that silently fails to restore a value is a data-loss bug, and it is testable here.

#include <doctest/doctest.h>

#include "Asset/AssetHandle.h"
#include "Core/UUID.h"
#include "Editor/CommandHistory.h"
#include "Editor/EditorCommand.h"
#include "Editor/SceneCommands.h"
#include "Scene/Components/LightComponent.h"
#include "Scene/Components/MeshRendererComponent.h"
#include "Scene/Components/TagComponent.h"
#include "Scene/Components/TransformComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Math/Math.h"

#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    void ClearScene(Scene& scene)
    {
        for (const Entity entity : scene.GetAllEntities())
        {
            scene.DestroyEntity(entity);
        }
    }

    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }
}

TEST_SUITE_BEGIN("Editor");

TEST_CASE("a scene starts clean and an edit makes it dirty")
{
    Scene scene;
    ClearScene(scene);

    CHECK_FALSE(scene.IsDirty());

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Cube");
    history.Execute(SceneCommands::MakeSetTag(entity, "Cube", "Renamed"));

    CHECK(scene.IsDirty());
    CHECK(entity.GetName() == "Renamed");

    scene.ClearDirty();
    CHECK_FALSE(scene.IsDirty());
    CHECK(history.CanUndo());
}

TEST_CASE("a transform edit is undone and redone")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Mover");
    entity.Transform().Position = { 1.0f, 2.0f, 3.0f };

    const Math::Vec3 target(4.0f, 5.0f, 6.0f);
    const Math::Vec3 identity(0.0f);
    const Math::Vec3 unitScale(1.0f);

    history.Execute(SceneCommands::MakeSetTransform(entity, target, identity, unitScale));
    CHECK(Near(entity.Transform().Position.x, 4.0f));

    REQUIRE(history.Undo());
    CHECK(Near(entity.Transform().Position.x, 1.0f));
    CHECK(Near(entity.Transform().Position.y, 2.0f));

    REQUIRE(history.Redo());
    CHECK(Near(entity.Transform().Position.x, 4.0f));
}

TEST_CASE("a merged edit collapses into one history entry")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Dragged");

    // A gizmo drag reports a new value every frame; the user expects one Ctrl+Z, not one per frame.
    for (int step = 1; step <= 20; ++step)
    {
        history.ExecuteMerged(SceneCommands::MakeSetTransform(
            entity,
            { static_cast<float>(step), 0.0f, 0.0f },
            Math::Vec3(0.0f),
            Math::Vec3(1.0f)));
    }

    CHECK(history.GetUndoCount() == 1);
    CHECK(Near(entity.Transform().Position.x, 20.0f));

    REQUIRE(history.Undo());
    CHECK(Near(entity.Transform().Position.x, 0.0f));
}

TEST_CASE("typed text merges while the user is still typing")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Untyped");

    const std::vector<std::string> keystrokes { "T", "Ty", "Typ", "Type", "Typed" };
    std::string previous = entity.GetName();
    for (const std::string& text : keystrokes)
    {
        entity.GetComponent<TagComponent>().Tag = text;
        history.ExecuteMerged(SceneCommands::MakeSetTag(entity, previous, text));
        previous = text;
    }

    CHECK(history.GetUndoCount() == 1);
    CHECK(entity.GetName() == "Typed");

    REQUIRE(history.Undo());
    CHECK(entity.GetName() == "Untyped");
}

TEST_CASE("a new edit drops the redo stack")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Entity");

    history.Execute(SceneCommands::MakeSetTag(entity, entity.GetName(), "First"));
    REQUIRE(history.Undo());
    CHECK(history.CanRedo());

    history.Execute(SceneCommands::MakeSetTag(entity, entity.GetName(), "Second"));
    CHECK_FALSE(history.CanRedo());
    CHECK(entity.GetName() == "Second");
}

TEST_CASE("the history keeps only the most recent entries")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    history.SetCapacity(4);
    Entity entity = scene.CreateEntity("Entity");

    for (int step = 0; step < 10; ++step)
    {
        history.Execute(SceneCommands::MakeSetTag(entity, entity.GetName(), "Name" + std::to_string(step)));
    }

    CHECK(history.GetUndoCount() == 4);

    int undoCount = 0;
    while (history.Undo())
    {
        ++undoCount;
    }
    CHECK(undoCount == 4);
}

TEST_CASE("a component value edit is reversible")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Light");
    entity.AddComponent<LightComponent>();

    history.Execute(SceneCommands::MakeSetLightIntensity(entity, 8.0f));
    CHECK(Near(entity.GetComponent<LightComponent>().Intensity, 8.0f));

    REQUIRE(history.Undo());
    CHECK(Near(entity.GetComponent<LightComponent>().Intensity, LightComponent().Intensity));

    REQUIRE(history.Redo());
    CHECK(Near(entity.GetComponent<LightComponent>().Intensity, 8.0f));
}

TEST_CASE("assigning a material is reversible without a renderer")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Cube");
    entity.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);

    const AssetHandle material = AssetHandle::From(UUID(0xCAFEBABE), AssetType::Material);
    history.Execute(SceneCommands::MakeSetMeshMaterial(entity, material));
    CHECK(entity.GetComponent<MeshRendererComponent>().Material == material);

    REQUIRE(history.Undo());
    CHECK_FALSE(entity.GetComponent<MeshRendererComponent>().Material.IsValid());

    REQUIRE(history.Redo());
    CHECK(entity.GetComponent<MeshRendererComponent>().Material == material);
}

TEST_CASE("changing the primitive restores the geometry on undo")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Cube");
    MeshRendererComponent& mesh = entity.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);
    REQUIRE(mesh.Mesh != nullptr);

    history.Execute(SceneCommands::MakeSetPrimitive(entity, static_cast<int>(PrimitiveMeshType::Sphere)));
    CHECK(mesh.Primitive == PrimitiveMeshType::Sphere);
    CHECK(mesh.Mesh != nullptr);
    // The geometry is replaced, not just the enum: the sphere's draw mode and bounds come with it.
    CHECK(mesh.Mesh->GetDrawMode() == MeshDrawMode::Triangles);

    REQUIRE(history.Undo());
    CHECK(mesh.Primitive == PrimitiveMeshType::Cube);
    CHECK(mesh.Mesh != nullptr);
}

TEST_CASE("adding and removing a component are both reversible")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Entity");
    REQUIRE_FALSE(entity.HasComponent<LightComponent>());

    const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<LightComponent>::value());
    REQUIRE(descriptor != nullptr);

    history.Execute(SceneCommands::MakeAddComponent(entity, descriptor->TypeID));
    REQUIRE(entity.HasComponent<LightComponent>());
    entity.GetComponent<LightComponent>().Intensity = 3.5f;

    // The removal records the entity, so the values come back rather than a fresh default.
    history.Execute(SceneCommands::MakeRemoveComponent(scene, entity, descriptor->TypeID));
    CHECK_FALSE(entity.HasComponent<LightComponent>());

    REQUIRE(history.Undo());
    REQUIRE(entity.HasComponent<LightComponent>());
    CHECK(Near(entity.GetComponent<LightComponent>().Intensity, 3.5f));

    REQUIRE(history.Undo());
    CHECK_FALSE(entity.HasComponent<LightComponent>());
}

TEST_CASE("destroying an entity can be undone with its components")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Doomed");
    entity.GetComponent<TagComponent>().Tag = "Keep me";
    entity.AddComponent<LightComponent>().Intensity = 7.0f;
    const UUID id = entity.GetUUID();

    // The command is built while the entity still exists - that is when the subtree can be recorded -
    // and the delete happens when the history executes it.
    Scope<EditorCommand> command = SceneCommands::MakeDestroyEntity(scene, entity);
    REQUIRE(command != nullptr);
    REQUIRE(scene.GetEntityByUUID(id));
    history.Execute(std::move(command));
    CHECK_FALSE(scene.GetEntityByUUID(id));

    REQUIRE(history.Undo());
    Entity restored = scene.GetEntityByUUID(id);
    REQUIRE(restored);
    CHECK(restored.GetName() == "Keep me");
    REQUIRE(restored.HasComponent<LightComponent>());
    CHECK(Near(restored.GetComponent<LightComponent>().Intensity, 7.0f));

    REQUIRE(history.Redo());
    CHECK_FALSE(scene.GetEntityByUUID(id));

    REQUIRE(history.Undo());
    CHECK(scene.GetEntityByUUID(id));
}

TEST_CASE("destroying a parent brings its children back on undo")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity parent = scene.CreateEntity("Parent");
    Entity child = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    const UUID parentId = parent.GetUUID();
    const UUID childId = child.GetUUID();

    REQUIRE(scene.GetChildren(parent).size() == 1);

    Scope<EditorCommand> command = SceneCommands::MakeDestroyEntity(scene, parent);
    REQUIRE(command != nullptr);
    history.Execute(std::move(command));
    CHECK_FALSE(scene.GetEntityByUUID(parentId));
    CHECK_FALSE(scene.GetEntityByUUID(childId));

    REQUIRE(history.Undo());
    Entity restoredParent = scene.GetEntityByUUID(parentId);
    Entity restoredChild = scene.GetEntityByUUID(childId);
    REQUIRE(restoredParent);
    REQUIRE(restoredChild);
    // The hierarchy survives the round trip, which a bare handle snapshot could not express.
    CHECK(scene.GetChildren(restoredParent).size() == 1);
}

TEST_CASE("a duplicate is undone as a creation")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity source = scene.CreateEntity("Original");
    source.Transform().Position = { 3.0f, 0.0f, 0.0f };

    Entity duplicate;
    Scope<EditorCommand> command = SceneCommands::MakeDuplicateEntity(scene, source, duplicate);
    REQUIRE(command != nullptr);
    REQUIRE(duplicate);
    history.Execute(std::move(command));

    CHECK(scene.GetAllEntities().size() == 2);
    const UUID duplicateId = duplicate.GetUUID();

    REQUIRE(history.Undo());
    CHECK(scene.GetAllEntities().size() == 1);
    CHECK_FALSE(scene.GetEntityByUUID(duplicateId));

    REQUIRE(history.Redo());
    CHECK(scene.GetAllEntities().size() == 2);
}

TEST_CASE("a command that would change nothing is not recorded")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Entity");
    entity.GetComponent<TagComponent>().Tag = "Same";
    entity.AddComponent<MeshRendererComponent>().SetPrimitive(PrimitiveMeshType::Cube);

    // The factories refuse a no-op, which is what keeps "click the gizmo without moving" out of the
    // undo stack.
    CHECK(SceneCommands::MakeSetTag(entity, "Same", "Same") == nullptr);
    CHECK(SceneCommands::MakeSetMeshMaterial(entity, AssetHandle::Invalid()) == nullptr);
    CHECK(SceneCommands::MakeSetPrimitive(entity, static_cast<int>(PrimitiveMeshType::Cube)) == nullptr);
    CHECK(SceneCommands::MakeSetMeshRoughness(entity, MeshRendererComponent().Roughness) == nullptr);
    CHECK(history.GetUndoCount() == 0);
}

TEST_CASE("a command aimed at a missing entity is a no-op rather than a crash")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Temporary");
    const UUID id = entity.GetUUID();

    const ComponentDescriptor* descriptor = ComponentRegistry::Find(entt::type_hash<LightComponent>::value());
    REQUIRE(descriptor != nullptr);

    Scope<EditorCommand> command = SceneCommands::MakeAddComponent(entity, descriptor->TypeID);
    REQUIRE(command != nullptr);

    // The entity is gone by the time the command runs, which is what an undo across a scene change
    // looks like.
    scene.DestroyEntity(entity);
    command->Apply(scene);
    command->Revert(scene);
    command->Commit(scene);
    CHECK_FALSE(scene.GetEntityByUUID(id));
}

TEST_CASE("clearing the history leaves the scene untouched")
{
    Scene scene;
    ClearScene(scene);

    CommandHistory history(scene);
    Entity entity = scene.CreateEntity("Entity");
    history.Execute(SceneCommands::MakeSetTag(entity, entity.GetName(), "Renamed"));

    history.Clear();
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
    CHECK(entity.GetName() == "Renamed");
}

TEST_SUITE_END();
