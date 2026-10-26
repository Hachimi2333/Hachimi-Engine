#pragma once

#include "Asset/AssetDatabase.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Scene.h"
#include "Scripting/Lua/LuaScriptRuntime.h"
#include "Serialization/SceneSerializer.h"
#include "Support/TestWorkspace.h"
#include "Utils/FileSystem.h"

#include <doctest/doctest.h>

namespace HachimiEngine::Tests
{
    // Real Lua, ECS and asset files, with no window or GPU resources. Owns the published database
    // for the whole session and restores the previous one even when a REQUIRE aborts a case.
    class ScriptTestScene
    {
    public:
        ScriptTestScene(const std::string& name, const std::string& source,
            std::vector<ScriptField> fields = {})
            : Directory(Workspace().PrepareDirectory(name)), OwnedScene(CreateRef<Scene>()),
              m_Previous(SceneSerializer::GetAssetDatabase())
        {
            REQUIRE(FileSystem::CreateDirectories(Directory / "Scripts"));
            Source = Directory / "Scripts" / "Test.lua";
            REQUIRE(FileSystem::WriteTextFile(Source, source));
            REQUIRE(Database.Refresh(Directory));
            SceneSerializer::SetAssetDatabase(&Database);
            Player = OwnedScene->CreateEntity("Player");
            auto& script = Player.AddComponent<ScriptComponent>().Scripts.emplace_back();
            script.Script = Database.GetHandleForPath(Source).value();
            script.DisplayName = "Test";
            script.Fields = std::move(fields);
        }

        ~ScriptTestScene()
        {
            if (Lua != nullptr) Lua->Shutdown(*OwnedScene);
            OwnedScene->OnRuntimeStop();
            SceneSerializer::SetAssetDatabase(m_Previous);
        }

        void StartLua()
        {
            Lua = CreateScope<LuaScriptRuntime>(*OwnedScene);
            Lua->CreateInstance(Player, 0, Source, "Test");
        }
        void Rewrite(const std::string& source)
        {
            const auto stamp = FileSystem::GetLastWriteTime(Source);
            REQUIRE(FileSystem::WriteTextFile(Source, source));
            std::filesystem::last_write_time(Source, stamp + std::chrono::seconds(1));
        }
        ScriptFieldData& Value(const std::string& name)
        {
            return ScriptFields::Find(Player.GetComponent<ScriptComponent>().Scripts[0].Fields, name)->Value;
        }
        float Number(const std::string& name) { return std::get<float>(Value(name)); }
        std::string Text(const std::string& name) { return std::get<std::string>(Value(name)); }

        static ScriptField Field(const std::string& name, ScriptFieldData value)
        {
            return { name, value, value };
        }

        std::filesystem::path Directory;
        std::filesystem::path Source;
        AssetDatabase Database;
        Ref<Scene> OwnedScene;
        Entity Player;
        Scope<LuaScriptRuntime> Lua;
    private:
        AssetDatabase* m_Previous;
    };
}
