// Script field runtime tests.
//
// These cases drive a real LuaScriptRuntime over a headless scene: the scripting backend needs a
// registry, a virtual file system and a Lua state, and none of those is a window or a GPU resource.
// What they pin down is the contract between the field table the script writes and the component the
// inspector edits - the design-time value a scene stores must survive everything a Play session does.

#include <doctest/doctest.h>

#include "Asset/AssetDatabase.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Scene.h"
#include "Scripting/Lua/LuaScriptRuntime.h"
#include "Serialization/SceneSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <filesystem>
#include <string>
#include <vector>

using namespace HachimiEngine;

namespace
{
    // The runtime reads the project's Assets directory to resolve module names, so a case that needs
    // require has to publish a database for as long as it runs - and put the static back afterwards,
    // because the next suite must not inherit it.
    class ScopedAssetDatabase
    {
    public:
        explicit ScopedAssetDatabase(AssetDatabase& database)
        {
            SceneSerializer::SetAssetDatabase(&database);
        }

        ~ScopedAssetDatabase()
        {
            SceneSerializer::SetAssetDatabase(nullptr);
        }

        ScopedAssetDatabase(const ScopedAssetDatabase&) = delete;
        ScopedAssetDatabase& operator=(const ScopedAssetDatabase&) = delete;
    };

    ScriptField MakeField(const std::string& name, ScriptFieldData value)
    {
        ScriptField field;
        field.Name = name;
        field.Value = value;
        field.DefaultValue = std::move(value);
        return field;
    }

    // One entity with one script slot and the runtime that slot runs in. The fields are declared
    // before the instance is created, which is the normal order: the scene carries them and the
    // runtime copies them into the script's table.
    class ScriptFieldScene
    {
    public:
        ScriptFieldScene(const std::string& directoryName, const std::string& source, std::vector<ScriptField> fields)
            : Directory(Tests::Workspace().PrepareDirectory(directoryName))
            , OwnedScene(CreateRef<Scene>())
        {
            const std::filesystem::path scriptPath = Directory / "Fields.lua";
            REQUIRE(FileSystem::WriteTextFile(scriptPath, source));

            Player = OwnedScene->CreateEntity("Player");
            auto& component = Player.AddComponent<ScriptComponent>();
            component.Scripts.emplace_back();
            component.Scripts[0].DisplayName = "Fields.lua";
            component.Scripts[0].Fields = std::move(fields);

            Runtime = CreateScope<LuaScriptRuntime>(*OwnedScene);

            // The asset database is what turns a reference into a path in the editor, so the test
            // hands the runtime the file it wrote itself.
            Runtime->CreateInstance(Player, 0, scriptPath, "Fields.lua");
        }

        ScriptComponent::ScriptReference& Reference()
        {
            return Player.GetComponent<ScriptComponent>().Scripts[0];
        }

        void Advance(float seconds = 1.0f / 60.0f)
        {
            Runtime->Update(Timestep(seconds), *OwnedScene);
        }

        void Stop()
        {
            Runtime->Shutdown(*OwnedScene);
        }

        std::filesystem::path Directory;
        Ref<Scene> OwnedScene;
        Entity Player;
        Scope<LuaScriptRuntime> Runtime;
    };
}

TEST_SUITE("Scripting")
{
    TEST_CASE("a script reads and writes its fields through self.fields")
    {
        ScriptFieldScene fixture("ScriptFieldRuntime",
            "local M = {}\n"
            "function M:OnCreate()\n"
            "  self.fields.Counter = self.fields.Counter + 1\n"
            "end\n"
            "function M:OnUpdate(dt)\n"
            "  self.fields.Counter = self.fields.Counter + 1\n"
            "  self.fields.Label = 'tick'\n"
            "end\n"
            "return M\n",
            { MakeField("Counter", 0.0f), MakeField("Label", std::string("idle")) });

        ScriptComponent::ScriptReference& reference = fixture.Reference();

        // OnCreate already saw the authored values, so the instance starts from what the scene says.
        CHECK(std::get<float>(reference.Fields[0].Value) == 1.0f);

        fixture.Advance();
        CHECK(std::get<float>(reference.Fields[0].Value) == 2.0f);
        CHECK(std::get<std::string>(reference.Fields[1].Value) == "tick");

        // The design-time value is what the scene stores and what the inspector shows as the
        // authored value: a run must not overwrite it.
        CHECK(std::get<float>(reference.Fields[0].DefaultValue) == 0.0f);
        CHECK(std::get<std::string>(reference.Fields[1].DefaultValue) == "idle");
        CHECK_FALSE(ScriptFields::IsDefaultValue(reference.Fields[0]));
        CHECK_FALSE(ScriptFields::IsDefaultValue(reference.Fields[1]));

        fixture.Stop();
    }

    TEST_CASE("a value written into the component while the game runs reaches the script")
    {
        ScriptFieldScene fixture("ScriptFieldRuntimePush",
            "local M = {}\n"
            "function M:OnUpdate(dt)\n"
            "  seen = self.fields.Speed\n"
            "  self.fields.Speed = self.fields.Speed * 2\n"
            "end\n"
            "return M\n",
            { MakeField("Speed", 3.0f) });

        ScriptComponent::ScriptReference& reference = fixture.Reference();

        fixture.Advance();
        CHECK(std::get<float>(reference.Fields[0].Value) == 6.0f);

        // An inspector edit during Play writes the runtime value, and the next frame has to hand it
        // to the script instead of overwriting it with the copy the script wrote itself.
        reference.Fields[0].Value = 21.0f;
        fixture.Advance();
        CHECK(std::get<float>(reference.Fields[0].Value) == 42.0f);

        // The authored value is untouched by any of it.
        CHECK(std::get<float>(reference.Fields[0].DefaultValue) == 3.0f);

        fixture.Stop();
    }

    TEST_CASE("a field added after the instance was created is pushed instead of read as nil")
    {
        ScriptFieldScene fixture("ScriptFieldRuntimeLate",
            "local M = {}\n"
            "function M:OnUpdate(dt)\n"
            "  self.fields.Speed = self.fields.Speed + 1\n"
            "end\n"
            "return M\n",
            {});

        // The instance was created from a slot with no fields, so the table starts empty and the
        // field has to arrive with the frame that reads it.
        fixture.Reference().Fields.push_back(MakeField("Speed", 10.0f));

        fixture.Advance();
        CHECK(std::get<float>(fixture.Reference().Fields[0].Value) == 11.0f);

        fixture.Stop();
    }

    TEST_CASE("a script that writes the wrong type is refused instead of converted")
    {
        ScriptFieldScene fixture("ScriptFieldRuntimeTypes",
            "local M = {}\n"
            "function M:OnUpdate(dt)\n"
            "  self.fields.Speed = 'fast'\n"
            "  self.fields.Undeclared = 1\n"
            "end\n"
            "return M\n",
            { MakeField("Speed", 5.0f) });

        fixture.Advance();

        // The mistake is reported and the field keeps what it had: a string is not silently turned
        // into a number behind the script's back.
        CHECK(std::get<float>(fixture.Reference().Fields[0].Value) == 5.0f);
        CHECK(std::get<float>(fixture.Reference().Fields[0].DefaultValue) == 5.0f);

        fixture.Stop();
    }

    TEST_CASE("a script instance loads a sibling module with require")
    {
        const std::filesystem::path assets = Tests::Workspace().PrepareDirectory("ScriptRequireInstance");
        REQUIRE(FileSystem::CreateDirectories(assets / "Scripts"));
        REQUIRE(FileSystem::WriteTextFile(assets / "Scripts" / "Helper.lua", "return { bonus = 5 }\n"));

        // require resolves under <Assets>/Scripts, and the asset database is what tells the runtime
        // where Assets is - the same source the editor and the packaged player use.
        AssetDatabase database;
        database.Refresh(assets);
        const ScopedAssetDatabase published(database);

        ScriptFieldScene fixture("ScriptRequireInstanceFixtures",
            "local helper = require('Helper')\n"
            "local M = {}\n"
            "function M:OnCreate()\n"
            "  self.fields.Total = self.fields.Total + helper.bonus\n"
            "end\n"
            "return M\n",
            { MakeField("Total", 1.0f) });

        CHECK(std::get<float>(fixture.Reference().Fields[0].Value) == 6.0f);

        fixture.Advance();
        fixture.Stop();
    }

    TEST_CASE("self exposes the entity, the scene and the time table")
    {
        ScriptFieldScene fixture("ScriptFieldRuntimeSelf",
            "local M = {}\n"
            "function M:OnCreate()\n"
            "  self.fields.Name = self.entity:GetName()\n"
            "  self.fields.Scene = self.scene:GetName()\n"
            "  self.fields.HasTime = (self.time ~= nil) and (self.time.DeltaTime ~= nil)\n"
            "end\n"
            "return M\n",
            { MakeField("Name", std::string()), MakeField("Scene", std::string()), MakeField("HasTime", false) });

        ScriptComponent::ScriptReference& reference = fixture.Reference();

        CHECK(std::get<std::string>(reference.Fields[0].Value) == "Player");
        CHECK(std::get<std::string>(reference.Fields[1].Value) == fixture.OwnedScene->GetName());
        CHECK(std::get<bool>(reference.Fields[2].Value));

        fixture.Advance();
        fixture.Stop();
    }
}
