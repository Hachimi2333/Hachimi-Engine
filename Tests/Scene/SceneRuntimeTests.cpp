// SceneRuntime's simulation and camera extraction are CPU-only; presentation is lazy and unused.
#include <doctest/doctest.h>

#include "Scene/Scene.h"
#include "Core/Input.h"
#include "Scene/SceneRuntime.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Serialization/SceneSerializer.h"
#include "Support/TestWorkspace.h"
#include "Support/ScriptTestScene.h"
#include "Packaging/PackageWriter.h"
#include "Utils/VirtualFileSystem.h"

using namespace HachimiEngine;

namespace
{
    class StepCounter final : public SceneSystem
    {
    public:
        std::string_view GetName() const override { return "StepCounter"; }
        ScenePhase GetPhase() const override { return ScenePhase::FixedUpdate; }
        void OnUpdate(Scene&, Timestep) override { ++Count; }
        int Count = 0;
    };

    Entity AddBody(Scene& scene)
    {
        auto body = scene.CreateEntity("Body");
        body.Transform().Position = { 0, 8, 0 };
        body.AddComponent<RigidbodyComponent>();
        body.AddComponent<ColliderComponent>();
        return body;
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("runtime attach and detach are idempotent and enforce exclusive scene ownership")
    {
        auto scene = CreateRef<Scene>();
        SceneRuntime first;
        SceneRuntime second;
        CHECK_FALSE(first.Attach(nullptr));
        REQUIRE(first.Attach(scene));
        auto* physics = scene->FindSystem<PhysicsSystem>();
        CHECK(first.Attach(scene));
        CHECK(scene->FindSystem<PhysicsSystem>() == physics);
        CHECK_FALSE(second.Attach(scene));
        CHECK(first.IsRunning());
        first.Detach();
        first.Detach();
        CHECK_FALSE(first.IsRunning());
        CHECK_FALSE(scene->IsRuntimeRunning());
        CHECK_FALSE(scene->IsPhysicsRunning());
        CHECK(scene->GetInputMap() == nullptr);
        CHECK(second.Attach(scene));
    }

    TEST_CASE("runtime pause freezes simulation and Step advances exactly one fixed step at time scale zero")
    {
        auto scene = CreateRef<Scene>();
        auto body = AddBody(*scene);
        auto& counter = scene->AddSystem<StepCounter>();
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(scene));
        runtime.Update(Timestep(1.0f / 60.0f));
        CHECK(counter.Count == 1);
        runtime.SetPaused(true);
        const auto position = body.Transform().Position;
        for (int i = 0; i < 10; ++i) runtime.Update(Timestep(0.1f));
        CHECK(counter.Count == 1);
        CHECK(body.Transform().Position == position);
        runtime.GetTime().SetTimeScale(0.0f);
        runtime.Step();
        CHECK(counter.Count == 2);
        CHECK(body.Transform().Position.y < position.y);
        CHECK(runtime.IsPaused());
        CHECK(runtime.GetTime().GetTimeScale() == 0.0f);
        CHECK(scene->GetInterpolationAlpha() == 1.0f);
        runtime.SetPaused(false);
        runtime.Update(Timestep(0.1f));
        CHECK(counter.Count == 2);
        runtime.GetTime().SetTimeScale(1.0f);
        runtime.Update(Timestep(1.0f / 60.0f));
        CHECK(counter.Count == 3);
    }

    TEST_CASE("two runtimes own independent clocks accumulators and physical worlds")
    {
        auto a = CreateRef<Scene>();
        auto b = CreateRef<Scene>();
        auto bodyA = AddBody(*a);
        auto bodyB = AddBody(*b);
        SceneRuntime first;
        SceneRuntime second;
        REQUIRE(first.Attach(a));
        REQUIRE(second.Attach(b));
        CHECK(a->FindSystem<PhysicsSystem>()->GetWorld() != b->FindSystem<PhysicsSystem>()->GetWorld());
        first.Update(Timestep(1.0f / 120.0f));
        second.Update(Timestep(1.0f / 60.0f));
        CHECK(bodyA.Transform().Position.y == 8.0f);
        CHECK(bodyB.Transform().Position.y < 8.0f);
        CHECK(a->GetInterpolationAlpha() == doctest::Approx(0.5f));
        first.Update(Timestep(1.0f / 120.0f));
        CHECK(bodyA.Transform().Position.y == doctest::Approx(bodyB.Transform().Position.y));
    }

    TEST_CASE("runtime evaluates named input before simulation and clears it on focus loss")
    {
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(CreateRef<Scene>()));
        InputSnapshot snapshot;
        snapshot.Keys.resize(Input::KeyCount);
        snapshot.Keys[Key::W] = true;
        runtime.Update(Timestep(0.01f), snapshot);
        CHECK(runtime.GetInputMap().GetActionPressed("MoveForward"));
        CHECK(runtime.GetInputMap().GetAxis("MoveY") == 1.0f);
        runtime.Update(Timestep(0.01f), snapshot);
        CHECK_FALSE(runtime.GetInputMap().GetActionPressed("MoveForward"));
        CHECK(runtime.GetInputMap().GetActionHeld("MoveForward"));
        runtime.SetInputEnabled(false);
        runtime.Update(Timestep(0.01f), snapshot);
        CHECK_FALSE(runtime.GetInputMap().GetActionHeld("MoveForward"));
        CHECK(runtime.GetInputMap().GetAxis("MoveY") == 0.0f);
    }

    TEST_CASE("a serialized player scene and the editor clone produce the same runtime result")
    {
        const auto directory = Tests::Workspace().PrepareDirectory("RuntimeParity");
        auto editorScene = CreateRef<Scene>();
        const auto body = AddBody(*editorScene);
        auto clone = editorScene->Clone();
        REQUIRE(SceneSerializer(editorScene).Serialize((directory / "Game.hscene").string()));
        auto playerScene = CreateRef<Scene>();
        REQUIRE(SceneSerializer(playerScene).Deserialize((directory / "Game.hscene").string()));
        SceneRuntime editor;
        SceneRuntime player;
        REQUIRE(editor.Attach(clone));
        REQUIRE(player.Attach(playerScene));
        for (int frame = 0; frame < 30; ++frame)
        {
            editor.Update(Timestep(1.0f / 60.0f));
            player.Update(Timestep(1.0f / 60.0f));
        }
        const auto simulatedA = clone->GetEntityByUUID(body.GetUUID());
        const auto simulatedB = playerScene->GetEntityByUUID(body.GetUUID());
        CHECK(simulatedA.Transform().Position.y == doctest::Approx(simulatedB.Transform().Position.y));
        const auto editorView = editor.BuildPrimaryCameraView(16.0f / 9.0f);
        const auto playerView = player.BuildPrimaryCameraView(16.0f / 9.0f);
        CHECK(editorView.CameraPosition == playerView.CameraPosition);
        CHECK(editorView.ViewProjection == playerView.ViewProjection);
        CHECK(body.Transform().Position.y == 8.0f);
    }
}

TEST_SUITE("Scene")
{
    TEST_CASE("packaged scene script fields and input map match an editor runtime without extraction")
    {
        Tests::ScriptTestScene fixture("PackagedRuntimeParity", R"(
local M = {}
function M:OnUpdate(dt)
    self.fields.Count = self.fields.Count + 1
    self.entity:Translate(HE.Math.Vec3(HE.Input.GetAxis('MoveY') * dt, 0, 0))
end
return M
)", { Tests::ScriptTestScene::Field("Count", 0.0f) });
        const auto map = InputMap::MakeDefault();
        REQUIRE(FileSystem::WriteTextFile(fixture.Directory / "Input.hinput", map.SaveToText()));
        REQUIRE(FileSystem::CreateDirectories(fixture.Directory / "Scenes"));
        REQUIRE(SceneSerializer(fixture.OwnedScene).Serialize((fixture.Directory / "Scenes" / "Game.hscene").string()));
        REQUIRE(fixture.Database.Refresh(fixture.Directory));
        SceneRuntime editor;
        editor.SetInputMap(map);
        REQUIRE(editor.Attach(fixture.OwnedScene->Clone()));

        const auto output = Tests::Workspace().PrepareDirectory("RuntimePackageOutput");
        PackageWriter writer;
        PackageWriterSettings settings;
        settings.UseDictionary = false;
        REQUIRE(writer.Open(output / "Data.hpak", settings));
        for (const auto& file : FileSystem::GetFilesRecursive(fixture.Directory))
            REQUIRE(writer.AddFile(std::filesystem::path("Assets") / file.lexically_relative(fixture.Directory), file));
        PackageBuildReport report;
        REQUIRE(writer.Finalize(report));
        const auto mount = output / "VirtualRoot";
        struct MountLifetime { ~MountLifetime() { VirtualFileSystem::UnmountAll(); } } mountLifetime;
        REQUIRE(VirtualFileSystem::MountArchive(output / "Data.hpak", mount));
        AssetDatabase database;
        REQUIRE(database.Refresh(mount / "Assets"));
        struct DatabaseLifetime
        {
            AssetDatabase* Previous;
            ~DatabaseLifetime() { SceneSerializer::SetAssetDatabase(Previous); }
        } databaseLifetime { SceneSerializer::GetAssetDatabase() };
        SceneSerializer::SetAssetDatabase(&database);
        auto loaded = CreateRef<Scene>();
        REQUIRE(SceneSerializer(loaded).Deserialize((mount / "Assets/Scenes/Game.hscene").string()));
        SceneRuntime player;
        player.SetInputMap(InputMap::LoadProject(mount / "Assets"));
        REQUIRE(player.Attach(loaded));
        InputSnapshot snapshot;
        snapshot.Keys.resize(Input::KeyCount);
        snapshot.Keys[Key::W] = true;
        for (int frame = 0; frame < 40; ++frame)
        {
            editor.Update(Timestep(1.0f / 60.0f), snapshot);
            player.Update(Timestep(1.0f / 60.0f), snapshot);
        }
        const auto fromEditor = editor.GetScene()->GetEntityByUUID(fixture.Player.GetUUID());
        const auto fromPlayer = loaded->GetEntityByUUID(fixture.Player.GetUUID());
        REQUIRE(fromEditor);
        REQUIRE(fromPlayer);
        CHECK(fromEditor.Transform().Position == fromPlayer.Transform().Position);
        CHECK(fromPlayer.Transform().Position.x > 0.0f);
        const auto& fields = fromPlayer.GetComponent<ScriptComponent>().Scripts[0].Fields;
        CHECK(std::get<float>(fields[0].Value) == 40.0f);
        CHECK_FALSE(FileSystem::Exists(mount / "Assets/Scripts/Test.lua"));
    }
}
