// Lua queries and motion calls use a real CPU Box3D world; no graphics backend is initialized.
#include <doctest/doctest.h>

#include "Physics/PhysicsWorld.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scene/Components/RigidbodyComponent.h"
#include "Scene/Systems/PhysicsSystem.h"
#include "Support/ScriptTestScene.h"

using namespace HachimiEngine;
using Tests::ScriptTestScene;

TEST_SUITE("Scripting")
{
    TEST_CASE("Lua physics safely returns empty results without a world")
    {
        ScriptTestScene fixture("LuaPhysicsAbsent", R"(
local M = {}
function M:OnCreate()
    local v = HE.Math.Vec3(0, 0, 0)
    self.fields.Safe = HE.Physics.Raycast(v, HE.Math.Vec3(0, -1, 0), 20) == nil
        and #HE.Physics.OverlapSphere(v, 1) == 0
    self.entity:ApplyImpulse(v)
    self.fields.Speed = HE.Math.Length(self.entity:GetLinearVelocity())
end
return M
)", { ScriptTestScene::Field("Safe", false), ScriptTestScene::Field("Speed", 9.0f) });
        fixture.StartLua();
        CHECK(std::get<bool>(fixture.Value("Safe")));
        CHECK(fixture.Number("Speed") == 0.0f);
    }

    TEST_CASE("Lua raycast motion teleport and trigger bindings reach the scene physics world")
    {
        ScriptTestScene fixture("LuaPhysicsWorld", R"(
local M = {}
function M:OnCreate()
    local v = HE.Math.Vec3
    local hit = HE.Physics.Raycast(v(0, 5, 0), v(0, -1, 0), 10)
    self.fields.Hit = hit and hit.entity:GetName() or 'none'
    self.fields.Distance = hit and hit.distance or 0
    self.entity:ApplyImpulse(v(2, 0, 0))
    self.fields.Speed = self.entity:GetLinearVelocity().x
    self.entity:SetAngularVelocity(v(0, 2, 0))
    self.entity:Teleport(v(3, 4, 5), v(0, 90, 0))
    self.entity:SetTriggered(true)
    self.fields.Trigger = self.entity:IsTriggered()
end
return M
)", { ScriptTestScene::Field("Hit", std::string()), ScriptTestScene::Field("Distance", 0.0f),
            ScriptTestScene::Field("Speed", 0.0f), ScriptTestScene::Field("Trigger", false) });
        fixture.Player.AddComponent<RigidbodyComponent>();
        // The rigidbody's default descriptor is only used by the generic add path.
        if (!fixture.Player.HasComponent<ColliderComponent>()) fixture.Player.AddComponent<ColliderComponent>();
        auto& physics = fixture.OwnedScene->AddSystem<PhysicsSystem>();
        fixture.StartLua();
        CHECK(fixture.Text("Hit") == "Player");
        CHECK(fixture.Number("Distance") == doctest::Approx(4.5f));
        CHECK(fixture.Number("Speed") > 0.0f);
        CHECK(fixture.Player.Transform().Position == Math::Vec3(3, 4, 5));
        CHECK(std::get<bool>(fixture.Value("Trigger")));
        CHECK(physics.GetWorld()->GetAngularVelocity(static_cast<uint64_t>(fixture.Player.GetHandle())).y == doctest::Approx(2.0f));
    }
}
