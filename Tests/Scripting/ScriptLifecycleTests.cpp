// Lifecycle, reload and event order use real Lua over headless scenes and deterministic file stamps.
#include <doctest/doctest.h>

#include "Scene/SceneRuntime.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Events/PhysicsSceneEvents.h"
#include "Support/ScriptTestScene.h"

using namespace HachimiEngine;
using Tests::ScriptTestScene;

TEST_SUITE("Scripting")
{
    TEST_CASE("script FixedUpdate matches simulation steps and commands flush before LateUpdate")
    {
        ScriptTestScene fixture("LuaFixedLifecycle", R"(
local M = {}
function M:OnCreate() self.spawned = self.scene:CreateEntity('Spawned') end
function M:FixedUpdate(dt) self.fields.Fixed = self.fields.Fixed + 1 end
function M:OnUpdate(dt)
    self.fields.Updates = self.fields.Updates + 1
    self.fields.Pending = self.spawned:IsPending()
end
return M
)", { ScriptTestScene::Field("Fixed", 0.0f), ScriptTestScene::Field("Updates", 0.0f), ScriptTestScene::Field("Pending", false) });
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(fixture.OwnedScene));
        runtime.Update(Timestep(1.0f / 30.0f));
        CHECK(fixture.Number("Fixed") == 2.0f);
        CHECK(fixture.Number("Updates") == 1.0f);
        CHECK(std::get<bool>(fixture.Value("Pending")));
        CHECK(fixture.OwnedScene->GetScriptCommands().IsEmpty());
        runtime.Update(Timestep(0.0f));
        CHECK_FALSE(std::get<bool>(fixture.Value("Pending")));
        CHECK(fixture.Number("Fixed") == 2.0f);
    }

    TEST_CASE("script collision and trigger callbacks arrive in order before OnUpdate")
    {
        ScriptTestScene fixture("LuaEventLifecycle", R"(
local M = {}
function M:OnCollisionEnter(other, contact)
    self.fields.Trace = self.fields.Trace .. 'C'
    self.fields.Other = other:GetName()
    self.fields.Normal = contact.normal
end
function M:OnCollisionExit(other) self.fields.Trace = self.fields.Trace .. 'c' end
function M:OnTriggerEnter(other) self.fields.Trace = self.fields.Trace .. 'T' end
function M:OnTriggerExit(other) self.fields.Trace = self.fields.Trace .. 't' end
function M:OnUpdate(dt) self.fields.Trace = self.fields.Trace .. 'U' end
return M
)", { ScriptTestScene::Field("Trace", std::string()), ScriptTestScene::Field("Other", std::string()),
            ScriptTestScene::Field("Normal", Math::Vec3(0.0f)) });
        auto other = fixture.OwnedScene->CreateEntity("Other");
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(fixture.OwnedScene));
        const auto key = static_cast<uint64_t>(fixture.Player.GetHandle());
        const auto otherKey = static_cast<uint64_t>(other.GetHandle());
        SceneCollisionEvent collision;
        collision.Begins.push_back({ key, otherKey, { 0, 0, 0 }, { 0, 1, 0 } });
        collision.Ends.push_back({ key, otherKey });
        fixture.OwnedScene->GetDispatcher().trigger(collision);
        SceneTriggerEvent trigger;
        trigger.Begins.push_back({ key, otherKey });
        trigger.Ends.push_back({ key, otherKey });
        fixture.OwnedScene->GetDispatcher().trigger(trigger);
        CHECK(fixture.Text("Trace").empty());
        runtime.Update(Timestep(0));
        CHECK(fixture.Text("Trace") == "CcTtU");
        CHECK(fixture.Text("Other") == "Other");
        CHECK(std::get<Math::Vec3>(fixture.Value("Normal")) == Math::Vec3(0, 1, 0));
        fixture.OwnedScene->GetDispatcher().trigger(collision);
        fixture.OwnedScene->DestroyEntity(other);
        runtime.Update(Timestep(0));
        CHECK(fixture.Text("Trace") == "CcTtUU");
    }

    TEST_CASE("Lua callback errors recover on the next invocation")
    {
        ScriptTestScene fixture("LuaRecoverableError", R"(
local attempts = 0
local M = {}
function M:OnUpdate(dt)
    attempts = attempts + 1
    if attempts == 1 then error('temporary failure') end
    self.fields.Count = attempts
end
return M
)", { ScriptTestScene::Field("Count", 0.0f) });
        fixture.StartLua();
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 0.0f);
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 2.0f);
    }

    TEST_CASE("hot reload preserves current fields and environment and calls destroy create reload")
    {
        ScriptTestScene fixture("LuaHotReload", R"(
persistent = (persistent or 0) + 1
local M = {}
function M:OnCreate() self.fields.Trace = self.fields.Trace .. 'C' end
function M:OnDestroy() self.fields.Trace = self.fields.Trace .. 'D' end
function M:OnUpdate(dt) self.fields.Speed = self.fields.Speed + 1 end
return M
)", { ScriptTestScene::Field("Trace", std::string()), ScriptTestScene::Field("Speed", 5.0f),
            ScriptTestScene::Field("Environment", 0.0f) });
        fixture.StartLua();
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        fixture.Rewrite(R"(
persistent = persistent + 1
local M = {}
function M:OnCreate() self.fields.Trace = self.fields.Trace .. 'N' end
function M:OnReload()
    self.fields.Trace = self.fields.Trace .. 'R'
    self.fields.Environment = persistent
end
function M:OnUpdate(dt) self.fields.Speed = self.fields.Speed + 10 end
return M
)");
        fixture.Lua->PollHotReload(0.5f, *fixture.OwnedScene);
        CHECK(fixture.Text("Trace") == "CDNR");
        CHECK(fixture.Number("Speed") == 6.0f);
        CHECK(fixture.Number("Environment") == 2.0f);
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Speed") == 16.0f);
        fixture.Rewrite("this is not Lua");
        fixture.Lua->PollHotReload(0.5f, *fixture.OwnedScene);
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Speed") == 26.0f);
    }

    TEST_CASE("a script that initially fails to load can recover through hot reload")
    {
        ScriptTestScene fixture("LuaBrokenLoad", "this is not Lua", { ScriptTestScene::Field("Count", 0.0f) });
        fixture.StartLua();
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        fixture.Rewrite("return { OnCreate = function(self) self.fields.Count = 7 end }");
        fixture.Lua->PollHotReload(0.5f, *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 7.0f);
    }
}

TEST_SUITE("Scripting")
{
    TEST_CASE("changing a required module reloads dependent instances")
    {
        ScriptTestScene fixture("LuaDependencyReload", R"(
local helper = require('Helper')
local M = {}
function M:OnUpdate(dt) self.fields.Count = helper.value end
return M
)", { ScriptTestScene::Field("Count", 0.0f) });
        const auto helper = fixture.Directory / "Scripts" / "Helper.lua";
        REQUIRE(FileSystem::WriteTextFile(helper, "return { value = 4 }"));
        fixture.StartLua();
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 4.0f);
        auto stamp = FileSystem::GetLastWriteTime(helper);
        REQUIRE(FileSystem::WriteTextFile(helper, "broken Lua source"));
        std::filesystem::last_write_time(helper, stamp + std::chrono::seconds(1));
        fixture.Lua->PollHotReload(0.5f, *fixture.OwnedScene);
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 4.0f);
        stamp = FileSystem::GetLastWriteTime(helper);
        REQUIRE(FileSystem::WriteTextFile(helper, "return { value = 9 }"));
        std::filesystem::last_write_time(helper, stamp + std::chrono::seconds(1));
        fixture.Lua->PollHotReload(0.5f, *fixture.OwnedScene);
        fixture.Lua->Update(Timestep(0.01f), *fixture.OwnedScene);
        CHECK(fixture.Number("Count") == 9.0f);
    }

    TEST_CASE("Lua TimeScale assignment and setter control the session clock")
    {
        ScriptTestScene fixture("LuaSessionTime", R"(
local M = {}
function M:OnUpdate(dt)
    self.fields.Count = self.fields.Count + 1
    if self.fields.Count == 1 then HE.Time.SetTimeScale(0.5)
    else HE.Time.TimeScale = 0 end
end
return M
)", { ScriptTestScene::Field("Count", 0.0f) });
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(fixture.OwnedScene));
        runtime.Update(Timestep(0.01f));
        CHECK(runtime.GetTime().GetTimeScale() == 0.5f);
        runtime.Update(Timestep(0.01f));
        CHECK(runtime.GetTime().GetTimeScale() == 0.0f);
    }
}

TEST_SUITE("Scripting")
{
    TEST_CASE("a real physics trigger reaches Lua once on entering and once on leaving")
    {
        ScriptTestScene fixture("LuaPhysicalTrigger", R"(
local M = {}
function M:OnTriggerEnter(other) self.fields.Enter = self.fields.Enter + 1 end
function M:OnTriggerExit(other) self.fields.Exit = self.fields.Exit + 1 end
return M
)", { ScriptTestScene::Field("Enter", 0.0f), ScriptTestScene::Field("Exit", 0.0f) });
        fixture.OwnedScene->GetPhysicsSettings().Gravity = { 0, 0, 0 };
        fixture.Player.Transform().Position = { 100, 5, 0 };
        fixture.Player.AddComponent<RigidbodyComponent>();
        fixture.Player.AddComponent<ColliderComponent>();
        auto trigger = fixture.OwnedScene->CreateEntity("Sensor");
        trigger.Transform().Position = { 100, 5, 0 };
        trigger.AddComponent<RigidbodyComponent>().Type = RigidbodyComponent::RigidbodyType::Static;
        auto& shape = trigger.AddComponent<ColliderComponent>();
        shape.IsTrigger = true;
        shape.HalfExtents = { 2, 2, 2 };
        SceneRuntime runtime;
        REQUIRE(runtime.Attach(fixture.OwnedScene));
        for (int frame = 0; frame < 5; ++frame) runtime.Update(Timestep(1.0f / 60.0f));
        CHECK(fixture.Number("Enter") == 1.0f);
        CHECK(fixture.Number("Exit") == 0.0f);
        auto* world = fixture.OwnedScene->FindSystem<PhysicsSystem>()->GetWorld();
        world->Teleport(static_cast<uint64_t>(fixture.Player.GetHandle()), { 110, 5, 0 }, Math::Quat(1, 0, 0, 0), *fixture.OwnedScene);
        for (int frame = 0; frame < 5; ++frame) runtime.Update(Timestep(1.0f / 60.0f));
        CHECK(fixture.Number("Enter") == 1.0f);
        CHECK(fixture.Number("Exit") == 1.0f);
    }
}
