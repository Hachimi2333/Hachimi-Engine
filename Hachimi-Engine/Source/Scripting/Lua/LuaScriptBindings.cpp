#include "Scripting/Lua/LuaScriptBindings.h"

#include "Core/Input.h"
#include "Scripting/ScriptEntity.h"
#include "Scripting/Lua/LuaPhysicsBindings.h"
#include "Core/KeyCodes.h"
#include "Core/Log.h"
#include "Core/MouseButtonCodes.h"
#include "Math/Math.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <array>
#include <optional>

namespace HachimiEngine
{
    namespace
    {
        void BindKeyConstants(sol::table& keyTable)
        {
#define HE_BIND_LUA_KEY(name) keyTable[#name] = Key::name

            HE_BIND_LUA_KEY(Space);
            HE_BIND_LUA_KEY(Apostrophe);
            HE_BIND_LUA_KEY(Comma);
            HE_BIND_LUA_KEY(Minus);
            HE_BIND_LUA_KEY(Period);
            HE_BIND_LUA_KEY(Slash);
            HE_BIND_LUA_KEY(D0);
            HE_BIND_LUA_KEY(D1);
            HE_BIND_LUA_KEY(D2);
            HE_BIND_LUA_KEY(D3);
            HE_BIND_LUA_KEY(D4);
            HE_BIND_LUA_KEY(D5);
            HE_BIND_LUA_KEY(D6);
            HE_BIND_LUA_KEY(D7);
            HE_BIND_LUA_KEY(D8);
            HE_BIND_LUA_KEY(D9);
            HE_BIND_LUA_KEY(Semicolon);
            HE_BIND_LUA_KEY(Equal);
            HE_BIND_LUA_KEY(A);
            HE_BIND_LUA_KEY(B);
            HE_BIND_LUA_KEY(C);
            HE_BIND_LUA_KEY(D);
            HE_BIND_LUA_KEY(E);
            HE_BIND_LUA_KEY(F);
            HE_BIND_LUA_KEY(G);
            HE_BIND_LUA_KEY(H);
            HE_BIND_LUA_KEY(I);
            HE_BIND_LUA_KEY(J);
            HE_BIND_LUA_KEY(K);
            HE_BIND_LUA_KEY(L);
            HE_BIND_LUA_KEY(M);
            HE_BIND_LUA_KEY(N);
            HE_BIND_LUA_KEY(O);
            HE_BIND_LUA_KEY(P);
            HE_BIND_LUA_KEY(Q);
            HE_BIND_LUA_KEY(R);
            HE_BIND_LUA_KEY(S);
            HE_BIND_LUA_KEY(T);
            HE_BIND_LUA_KEY(U);
            HE_BIND_LUA_KEY(V);
            HE_BIND_LUA_KEY(W);
            HE_BIND_LUA_KEY(X);
            HE_BIND_LUA_KEY(Y);
            HE_BIND_LUA_KEY(Z);
            HE_BIND_LUA_KEY(LeftBracket);
            HE_BIND_LUA_KEY(Backslash);
            HE_BIND_LUA_KEY(RightBracket);
            HE_BIND_LUA_KEY(GraveAccent);
            HE_BIND_LUA_KEY(Escape);
            HE_BIND_LUA_KEY(Enter);
            HE_BIND_LUA_KEY(Tab);
            HE_BIND_LUA_KEY(Backspace);
            HE_BIND_LUA_KEY(Insert);
            HE_BIND_LUA_KEY(Delete);
            HE_BIND_LUA_KEY(Right);
            HE_BIND_LUA_KEY(Left);
            HE_BIND_LUA_KEY(Down);
            HE_BIND_LUA_KEY(Up);
            HE_BIND_LUA_KEY(PageUp);
            HE_BIND_LUA_KEY(PageDown);
            HE_BIND_LUA_KEY(Home);
            HE_BIND_LUA_KEY(End);
            HE_BIND_LUA_KEY(CapsLock);
            HE_BIND_LUA_KEY(ScrollLock);
            HE_BIND_LUA_KEY(NumLock);
            HE_BIND_LUA_KEY(PrintScreen);
            HE_BIND_LUA_KEY(Pause);
            HE_BIND_LUA_KEY(F1);
            HE_BIND_LUA_KEY(F2);
            HE_BIND_LUA_KEY(F3);
            HE_BIND_LUA_KEY(F4);
            HE_BIND_LUA_KEY(F5);
            HE_BIND_LUA_KEY(F6);
            HE_BIND_LUA_KEY(F7);
            HE_BIND_LUA_KEY(F8);
            HE_BIND_LUA_KEY(F9);
            HE_BIND_LUA_KEY(F10);
            HE_BIND_LUA_KEY(F11);
            HE_BIND_LUA_KEY(F12);
            HE_BIND_LUA_KEY(LeftShift);
            HE_BIND_LUA_KEY(LeftControl);
            HE_BIND_LUA_KEY(LeftAlt);
            HE_BIND_LUA_KEY(RightShift);
            HE_BIND_LUA_KEY(RightControl);
            HE_BIND_LUA_KEY(RightAlt);

#undef HE_BIND_LUA_KEY
        }

        void BindMath(sol::state& state, sol::table& mathTable)
        {
            auto vec3Type = state.new_usertype<Math::Vec3>(
                "HEMathVec3",
                sol::constructors<Math::Vec3(), Math::Vec3(float, float, float)>(),
                "x", &Math::Vec3::x,
                "y", &Math::Vec3::y,
                "z", &Math::Vec3::z);

            vec3Type.set_function(sol::meta_function::addition, [](const Math::Vec3& lhs, const Math::Vec3& rhs)
            {
                return lhs + rhs;
            });
            vec3Type.set_function(sol::meta_function::subtraction, [](const Math::Vec3& lhs, const Math::Vec3& rhs)
            {
                return lhs - rhs;
            });
            vec3Type.set_function(sol::meta_function::multiplication, sol::overload(
                [](const Math::Vec3& lhs, const Math::Vec3& rhs)
                {
                    return lhs * rhs;
                },
                [](const Math::Vec3& lhs, float rhs)
                {
                    return lhs * rhs;
                }));
            vec3Type.set_function(sol::meta_function::division, sol::overload(
                [](const Math::Vec3& lhs, const Math::Vec3& rhs)
                {
                    return lhs / rhs;
                },
                [](const Math::Vec3& lhs, float rhs)
                {
                    return lhs / rhs;
                }));
            vec3Type.set_function(sol::meta_function::unary_minus, [](const Math::Vec3& value)
            {
                return -value;
            });

            mathTable.set_function("Vec3", [](float x, float y, float z)
            {
                return Math::Vec3(x, y, z);
            });
            mathTable.set_function("Length", [](const Math::Vec3& value)
            {
                return Math::Length(value);
            });
            mathTable.set_function("Normalize", [](const Math::Vec3& value)
            {
                return Math::Normalize(value);
            });
            mathTable.set_function("Dot", [](const Math::Vec3& lhs, const Math::Vec3& rhs)
            {
                return Math::Dot(lhs, rhs);
            });
            mathTable.set_function("Cross", [](const Math::Vec3& lhs, const Math::Vec3& rhs)
            {
                return Math::Cross(lhs, rhs);
            });
            mathTable.set_function("Clamp", [](float value, float minValue, float maxValue)
            {
                return Math::Clamp(value, minValue, maxValue);
            });
            mathTable.set_function("Lerp", [](const Math::Vec3& lhs, const Math::Vec3& rhs, float factor)
            {
                return Math::Mix(lhs, rhs, factor);
            });
            mathTable.set_function("Radians", [](float degrees)
            {
                return Math::Radians(degrees);
            });
            mathTable.set_function("Degrees", [](float radians)
            {
                return Math::Degrees(radians);
            });
        }
    }

    void AttachLuaScriptInstance(sol::table& moduleTable, Entity entity, Scene& scene, sol::state& state,
                                 sol::table& fieldsTable)
    {
        // `self` is the module table itself: the callbacks are called as module:OnUpdate(dt), so
        // the engine-provided names have to live next to the module's own functions.
        moduleTable["entity"] = ScriptEntity(entity, &scene);
        moduleTable["scene"] = state["HE"]["Scene"];
        moduleTable["time"] = state["HE"]["Time"];
        moduleTable["fields"] = fieldsTable;
    }

    void RegisterLuaBindings(sol::state& state, Scene& scene, sol::table& timeTable)
    {
        sol::table he = state.create_table();
        state["HE"] = he;

        he["Time"] = timeTable;

        sol::table log = state.create_table();
        he["Log"] = log;
        log.set_function("Info", [](const std::string& message)
        {
            HE_CORE_INFO("{}", message);
        });
        log.set_function("Warn", [](const std::string& message)
        {
            HE_CORE_WARN("{}", message);
        });
        log.set_function("Error", [](const std::string& message)
        {
            HE_CORE_ERROR("{}", message);
        });

        sol::table input = state.create_table();
        he["Input"] = input;
        input.set_function("IsKeyDown", [](int keyCode)
        {
            return Input::IsKeyPressed(keyCode);
        });
        input.set_function("IsMouseButtonDown", [](int mouseButton)
        {
            return Input::IsMouseButtonPressed(mouseButton);
        });
        input.set_function("GetMousePosition", []()
        {
            const auto position = Input::GetMousePosition();
            return sol::as_returns(std::array<float, 2> { position.first, position.second });
        });

        input.set_function("GetActionPressed", [&scene](const std::string& name) { auto* map = scene.GetInputMap(); return map && map->GetActionPressed(name); });
        input.set_function("GetActionHeld", [&scene](const std::string& name) { auto* map = scene.GetInputMap(); return map && map->GetActionHeld(name); });
        input.set_function("GetActionReleased", [&scene](const std::string& name) { auto* map = scene.GetInputMap(); return map && map->GetActionReleased(name); });
        input.set_function("GetAxis", [&scene](const std::string& name) { auto* map = scene.GetInputMap(); return map ? map->GetAxis(name) : 0.0f; });
        timeTable.set_function("SetTimeScale", [&scene, timeTable](float scale) mutable
        {
            scene.GetTime().SetTimeScale(scale);
            timeTable["TimeScale"] = scene.GetTime().GetTimeScale();
        });

        sol::table key = state.create_table();
        he["Key"] = key;
        BindKeyConstants(key);

        sol::table mouse = state.create_table();
        he["Mouse"] = mouse;
        mouse["ButtonLeft"] = Mouse::ButtonLeft;
        mouse["ButtonRight"] = Mouse::ButtonRight;
        mouse["ButtonMiddle"] = Mouse::ButtonMiddle;
        mouse["Button1"] = Mouse::Button1;
        mouse["Button2"] = Mouse::Button2;
        mouse["Button3"] = Mouse::Button3;
        mouse["Button4"] = Mouse::Button4;
        mouse["Button5"] = Mouse::Button5;
        mouse["Button6"] = Mouse::Button6;
        mouse["Button7"] = Mouse::Button7;
        mouse["Button8"] = Mouse::Button8;

        sol::table math = state.create_table();
        he["Math"] = math;
        BindMath(state, math);

        state.new_usertype<ScriptEntity>(
            "ScriptEntity",
            sol::no_constructor,
            "GetName", &ScriptEntity::GetName,
            "SetName", &ScriptEntity::SetName,
            "GetUUID", &ScriptEntity::GetUUID,
            "GetPosition", &ScriptEntity::GetPosition,
            "SetPosition", &ScriptEntity::SetPosition,
            "GetRotation", &ScriptEntity::GetRotation,
            "SetRotation", &ScriptEntity::SetRotation,
            "GetScale", &ScriptEntity::GetScale,
            "SetScale", &ScriptEntity::SetScale,
            "GetWorldPosition", &ScriptEntity::GetWorldPosition,
            "Translate", &ScriptEntity::Translate,
            "Rotate", &ScriptEntity::Rotate,
            "IsPending", &ScriptEntity::IsPending,
            "IsValid", &ScriptEntity::IsValid,
            "HasComponent", &ScriptEntity::HasComponent,
            "AddComponent", &ScriptEntity::AddComponent,
            "RemoveComponent", &ScriptEntity::RemoveComponent,
            "Destroy", &ScriptEntity::Destroy,
            "SetParent", &ScriptEntity::SetParent,
            "ClearParent", &ScriptEntity::ClearParent);

        RegisterLuaPhysicsBindings(state, he, scene);

        sol::table sceneTable = state.create_table();
        he["Scene"] = sceneTable;
        sceneTable.set_function("CreateEntity", [&scene](sol::object first, sol::optional<std::string> name)
        {
            // Both HE.Scene.CreateEntity(name) and self.scene:CreateEntity(name) are accepted.
            const std::string label = first.is<std::string>() ? first.as<std::string>() : name.value_or("Entity");
            return ScriptEntity(scene.GetScriptCommands().CreateEntity(label), &scene);
        });
        sceneTable.set_function("DestroyEntity", [](sol::object first, sol::optional<ScriptEntity> target)
        {
            if (first.is<ScriptEntity>()) first.as<ScriptEntity>().Destroy();
            else if (target) target->Destroy();
        });
        sceneTable.set_function("GetName", [&scene]()
        {
            return scene.GetName();
        });
        sceneTable.set_function("FindEntityByName", [&scene](const std::string& name) -> std::optional<ScriptEntity>
        {
            for (Entity entity : scene.GetAllEntities())
            {
                if (entity.GetName() == name)
                {
                    return ScriptEntity(entity, &scene);
                }
            }
            return std::nullopt;
        });
    }
}
