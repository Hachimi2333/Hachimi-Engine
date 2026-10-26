#include "Core/InputMap.h"
#include "Utils/VirtualFileSystem.h"

#include "Core/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* FormatVersionKey = "FormatVersion";
        constexpr int CurrentFormatVersion = 1;

        struct GamepadButtonNameEntry
        {
            const char* Name;
            int Index;
        };

        // Mirrors the GLFW gamepad button enumeration, which the engine's button indices follow.
        constexpr std::array<GamepadButtonNameEntry, GamepadButtonCount> GamepadButtonNames { {
            { "A", 0 },
            { "B", 1 },
            { "X", 2 },
            { "Y", 3 },
            { "LeftBumper", 4 },
            { "RightBumper", 5 },
            { "Back", 6 },
            { "Start", 7 },
            { "Guide", 8 },
            { "LeftThumb", 9 },
            { "RightThumb", 10 },
            { "DpadUp", 11 },
            { "DpadRight", 12 },
            { "DpadDown", 13 },
            { "DpadLeft", 14 }
        } };

        struct GamepadAxisNameEntry
        {
            const char* Name;
            int Index;
        };

        constexpr std::array<GamepadAxisNameEntry, GamepadAxisCount> GamepadAxisNames { {
            { "LeftX", 0 },
            { "LeftY", 1 },
            { "RightX", 2 },
            { "RightY", 3 },
            { "LeftTrigger", 4 },
            { "RightTrigger", 5 }
        } };

        struct KeyNameEntry
        {
            const char* Name;
            KeyCode Code;
        };

        // One table for both directions, so a name can never be parseable and unprintable at the
        // same time.
        constexpr KeyNameEntry KeyNames[] = {
#define HE_KEY_NAME(name) { #name, Key::name }
            HE_KEY_NAME(Space), HE_KEY_NAME(Apostrophe), HE_KEY_NAME(Comma), HE_KEY_NAME(Minus),
            HE_KEY_NAME(Period), HE_KEY_NAME(Slash),
            HE_KEY_NAME(D0), HE_KEY_NAME(D1), HE_KEY_NAME(D2), HE_KEY_NAME(D3), HE_KEY_NAME(D4),
            HE_KEY_NAME(D5), HE_KEY_NAME(D6), HE_KEY_NAME(D7), HE_KEY_NAME(D8), HE_KEY_NAME(D9),
            HE_KEY_NAME(Semicolon), HE_KEY_NAME(Equal),
            HE_KEY_NAME(A), HE_KEY_NAME(B), HE_KEY_NAME(C), HE_KEY_NAME(D), HE_KEY_NAME(E),
            HE_KEY_NAME(F), HE_KEY_NAME(G), HE_KEY_NAME(H), HE_KEY_NAME(I), HE_KEY_NAME(J),
            HE_KEY_NAME(K), HE_KEY_NAME(L), HE_KEY_NAME(M), HE_KEY_NAME(N), HE_KEY_NAME(O),
            HE_KEY_NAME(P), HE_KEY_NAME(Q), HE_KEY_NAME(R), HE_KEY_NAME(S), HE_KEY_NAME(T),
            HE_KEY_NAME(U), HE_KEY_NAME(V), HE_KEY_NAME(W), HE_KEY_NAME(X), HE_KEY_NAME(Y),
            HE_KEY_NAME(Z),
            HE_KEY_NAME(LeftBracket), HE_KEY_NAME(Backslash), HE_KEY_NAME(RightBracket),
            HE_KEY_NAME(GraveAccent), HE_KEY_NAME(Escape), HE_KEY_NAME(Enter), HE_KEY_NAME(Tab),
            HE_KEY_NAME(Backspace), HE_KEY_NAME(Insert), HE_KEY_NAME(Delete), HE_KEY_NAME(Right),
            HE_KEY_NAME(Left), HE_KEY_NAME(Down), HE_KEY_NAME(Up), HE_KEY_NAME(PageUp),
            HE_KEY_NAME(PageDown), HE_KEY_NAME(Home), HE_KEY_NAME(End), HE_KEY_NAME(CapsLock),
            HE_KEY_NAME(ScrollLock), HE_KEY_NAME(NumLock), HE_KEY_NAME(PrintScreen),
            HE_KEY_NAME(Pause),
            HE_KEY_NAME(F1), HE_KEY_NAME(F2), HE_KEY_NAME(F3), HE_KEY_NAME(F4), HE_KEY_NAME(F5),
            HE_KEY_NAME(F6), HE_KEY_NAME(F7), HE_KEY_NAME(F8), HE_KEY_NAME(F9), HE_KEY_NAME(F10),
            HE_KEY_NAME(F11), HE_KEY_NAME(F12),
            HE_KEY_NAME(LeftShift), HE_KEY_NAME(LeftControl), HE_KEY_NAME(LeftAlt),
            HE_KEY_NAME(RightShift), HE_KEY_NAME(RightControl), HE_KEY_NAME(RightAlt)
#undef HE_KEY_NAME
        };

        struct MouseNameEntry
        {
            const char* Name;
            MouseCode Code;
        };

        constexpr MouseNameEntry MouseNames[] = {
            { "Left", Mouse::ButtonLeft },
            { "Right", Mouse::ButtonRight },
            { "Middle", Mouse::ButtonMiddle },
            { "Button4", Mouse::Button4 },
            { "Button5", Mouse::Button5 },
            { "Button6", Mouse::Button6 },
            { "Button7", Mouse::Button7 },
            { "Button8", Mouse::Button8 }
        };

        const char* BindingKindName(InputBinding::Kind kind)
        {
            switch (kind)
            {
                case InputBinding::Kind::Key:
                    return "Key";
                case InputBinding::Kind::MouseButton:
                    return "MouseButton";
                case InputBinding::Kind::GamepadButton:
                    return "GamepadButton";
            }
            return "Key";
        }

        const char* SourceKindName(InputAxis::SourceKind kind)
        {
            switch (kind)
            {
                case InputAxis::SourceKind::None:
                    return "None";
                case InputAxis::SourceKind::Key:
                    return "Key";
                case InputAxis::SourceKind::MouseButton:
                    return "MouseButton";
                case InputAxis::SourceKind::GamepadAxis:
                    return "GamepadAxis";
                case InputAxis::SourceKind::MouseMotion:
                    return "MouseMotion";
                case InputAxis::SourceKind::MouseWheel:
                    return "MouseWheel";
            }
            return "None";
        }

        std::string BindingCodeName(const InputBinding& binding)
        {
            switch (binding.Type)
            {
                case InputBinding::Kind::Key:
                    return InputNames::KeyName(binding.Code);
                case InputBinding::Kind::MouseButton:
                    return InputNames::MouseName(static_cast<MouseCode>(binding.Code));
                case InputBinding::Kind::GamepadButton:
                    return InputNames::GamepadButtonName(binding.Code);
            }
            return {};
        }

        std::string SourceCodeName(const InputAxis::Source& source)
        {
            switch (source.Kind)
            {
                case InputAxis::SourceKind::Key:
                    return InputNames::KeyName(static_cast<KeyCode>(source.Code));
                case InputAxis::SourceKind::MouseButton:
                    return InputNames::MouseName(static_cast<MouseCode>(source.Code));
                case InputAxis::SourceKind::GamepadAxis:
                    return InputNames::GamepadAxisName(source.Code);
                case InputAxis::SourceKind::MouseMotion:
                case InputAxis::SourceKind::MouseWheel:
                case InputAxis::SourceKind::None:
                    // These read the device directly, so they carry no code.
                    return std::string();
            }
            return std::string();
        }

        bool ParseBindingCode(InputBinding::Kind kind, const std::string& name, int& outCode)
        {
            switch (kind)
            {
                case InputBinding::Kind::Key:
                {
                    KeyCode key = 0;
                    if (!InputNames::ParseKey(name, key))
                    {
                        return false;
                    }
                    outCode = key;
                    return true;
                }
                case InputBinding::Kind::MouseButton:
                {
                    MouseCode button = 0;
                    if (!InputNames::ParseMouseButton(name, button))
                    {
                        return false;
                    }
                    outCode = button;
                    return true;
                }
                case InputBinding::Kind::GamepadButton:
                    return InputNames::ParseGamepadButton(name, outCode);
            }
            return false;
        }

        bool ParseSourceCode(InputAxis::SourceKind kind, const std::string& name, int& outCode)
        {
            switch (kind)
            {
                case InputAxis::SourceKind::Key:
                {
                    KeyCode key = 0;
                    if (!InputNames::ParseKey(name, key))
                    {
                        return false;
                    }
                    outCode = key;
                    return true;
                }
                case InputAxis::SourceKind::MouseButton:
                {
                    MouseCode button = 0;
                    if (!InputNames::ParseMouseButton(name, button))
                    {
                        return false;
                    }
                    outCode = button;
                    return true;
                }
                case InputAxis::SourceKind::GamepadAxis:
                    return InputNames::ParseGamepadAxis(name, outCode);
                case InputAxis::SourceKind::MouseMotion:
                case InputAxis::SourceKind::MouseWheel:
                case InputAxis::SourceKind::None:
                    outCode = 0;
                    return true;
            }
            return false;
        }

        // Applies the dead zone and rescales the remainder, so a stick that just left the dead
        // zone starts from zero instead of jumping to the dead-zone threshold.
        float ApplyDeadZone(float value, float deadZone)
        {
            const float magnitude = std::fabs(value);
            if (magnitude <= deadZone)
            {
                return 0.0f;
            }

            const float clamped = std::min(magnitude, 1.0f);
            const float rescaled = (clamped - deadZone) / std::max(1.0f - deadZone, 1e-4f);
            return std::copysign(rescaled, value);
        }
    }

    namespace InputNames
    {
        const char* KeyName(KeyCode keyCode)
        {
            for (const KeyNameEntry& entry : KeyNames)
            {
                if (entry.Code == keyCode)
                {
                    return entry.Name;
                }
            }
            return "Unknown";
        }

        const char* MouseName(MouseCode button)
        {
            for (const MouseNameEntry& entry : MouseNames)
            {
                if (entry.Code == button)
                {
                    return entry.Name;
                }
            }
            return "Unknown";
        }

        const char* GamepadButtonName(int gamepadButton)
        {
            for (const GamepadButtonNameEntry& entry : GamepadButtonNames)
            {
                if (entry.Index == gamepadButton)
                {
                    return entry.Name;
                }
            }
            return "Unknown";
        }

        const char* GamepadAxisName(int gamepadAxis)
        {
            for (const GamepadAxisNameEntry& entry : GamepadAxisNames)
            {
                if (entry.Index == gamepadAxis)
                {
                    return entry.Name;
                }
            }
            return "Unknown";
        }

        bool ParseKey(const std::string& name, KeyCode& outKeyCode)
        {
            for (const KeyNameEntry& entry : KeyNames)
            {
                if (name == entry.Name)
                {
                    outKeyCode = entry.Code;
                    return true;
                }
            }
            return false;
        }

        bool ParseMouseButton(const std::string& name, MouseCode& outButton)
        {
            for (const MouseNameEntry& entry : MouseNames)
            {
                if (name == entry.Name)
                {
                    outButton = entry.Code;
                    return true;
                }
            }
            return false;
        }

        bool ParseGamepadButton(const std::string& name, int& outButton)
        {
            for (const GamepadButtonNameEntry& entry : GamepadButtonNames)
            {
                if (name == entry.Name)
                {
                    outButton = entry.Index;
                    return true;
                }
            }
            return false;
        }

        bool ParseGamepadAxis(const std::string& name, int& outAxis)
        {
            for (const GamepadAxisNameEntry& entry : GamepadAxisNames)
            {
                if (name == entry.Name)
                {
                    outAxis = entry.Index;
                    return true;
                }
            }
            return false;
        }

        bool ParseBindingKind(const std::string& name, InputBinding::Kind& outKind)
        {
            if (name == "Key")
            {
                outKind = InputBinding::Kind::Key;
                return true;
            }
            if (name == "MouseButton")
            {
                outKind = InputBinding::Kind::MouseButton;
                return true;
            }
            if (name == "GamepadButton")
            {
                outKind = InputBinding::Kind::GamepadButton;
                return true;
            }
            return false;
        }

        bool ParseSourceKind(const std::string& name, InputAxis::SourceKind& outKind)
        {
            if (name == "None")
            {
                outKind = InputAxis::SourceKind::None;
                return true;
            }
            if (name == "Key")
            {
                outKind = InputAxis::SourceKind::Key;
                return true;
            }
            if (name == "MouseButton")
            {
                outKind = InputAxis::SourceKind::MouseButton;
                return true;
            }
            if (name == "GamepadAxis")
            {
                outKind = InputAxis::SourceKind::GamepadAxis;
                return true;
            }
            if (name == "MouseMotion")
            {
                outKind = InputAxis::SourceKind::MouseMotion;
                return true;
            }
            if (name == "MouseWheel")
            {
                outKind = InputAxis::SourceKind::MouseWheel;
                return true;
            }
            return false;
        }
    }

    InputMap InputMap::LoadProject(const std::filesystem::path& assetsDirectory)
    {
        InputMap result = MakeDefault();
        const auto path = assetsDirectory / "Input.hinput";
        std::string source;
        if (!VirtualFileSystem::ReadTextFile(path, source))
            HE_CORE_INFO("No input map at '{}'; using built-in bindings", path.string());
        else if (!result.LoadFromText(source))
            HE_CORE_WARN("Invalid input map at '{}'; using built-in bindings", path.string());
        return result;
    }

    InputMap InputMap::MakeDefault()
    {
        const auto keyBinding = [](KeyCode code)
        {
            return InputBinding { InputBinding::Kind::Key, code };
        };
        const auto mouseBinding = [](MouseCode code)
        {
            return InputBinding { InputBinding::Kind::MouseButton, code };
        };
        const auto padBinding = [](int button)
        {
            return InputBinding { InputBinding::Kind::GamepadButton, button };
        };
        const auto keySource = [](KeyCode code)
        {
            return InputAxis::Source { InputAxis::SourceKind::Key, code, 1.0f };
        };
        const auto padAxisSource = [](int axis)
        {
            return InputAxis::Source { InputAxis::SourceKind::GamepadAxis, axis, 1.0f };
        };

        InputMap map;

        // A playable default: WASD + mouse look + space to jump, with the standard gamepad layout
        // alongside so a controller works without any project setup.
        map.m_Actions = {
            InputAction { "MoveForward", { keyBinding(Key::W), padBinding(11) } },
            InputAction { "MoveBack", { keyBinding(Key::S), padBinding(13) } },
            InputAction { "MoveLeft", { keyBinding(Key::A), padBinding(14) } },
            InputAction { "MoveRight", { keyBinding(Key::D), padBinding(12) } },
            InputAction { "Jump", { keyBinding(Key::Space), padBinding(0) } },
            InputAction { "PrimaryAction", { mouseBinding(Mouse::ButtonLeft), padBinding(2) } },
            InputAction { "SecondaryAction", { mouseBinding(Mouse::ButtonRight), padBinding(1) } },
            InputAction { "Look", { mouseBinding(Mouse::ButtonRight), padBinding(9) } },
            InputAction { "Pause", { keyBinding(Key::Escape), padBinding(7) } }
        };

        const auto makePairAxis = [](std::string name, InputAxis::Source positive, InputAxis::Source negative)
        {
            InputAxis axis;
            axis.Name = std::move(name);
            axis.Positive = positive;
            axis.Negative = negative;
            return axis;
        };

        // Keyboard and gamepad movement stay separate axes on purpose: a project combines them in
        // gameplay code, where "clamp(keyboard + stick)" is obvious. Folding them into one entry
        // would need the map to know how to blend two very different devices.
        map.m_Axes = {
            makePairAxis("MoveX", keySource(Key::D), keySource(Key::A)),
            makePairAxis("MoveY", keySource(Key::W), keySource(Key::S)),
            makePairAxis("GamepadMoveX", padAxisSource(0), InputAxis::Source {}),
            makePairAxis("GamepadMoveY", padAxisSource(1), InputAxis::Source {})
        };

        InputAxis lookX;
        lookX.Name = "LookX";
        lookX.Positive.Kind = InputAxis::SourceKind::MouseMotion;
        lookX.Positive.Scale = 1.0f;
        lookX.Gate = InputAxis::Source { InputAxis::SourceKind::MouseButton, Mouse::ButtonRight, 1.0f };
        map.m_Axes.push_back(std::move(lookX));

        InputAxis lookY;
        lookY.Name = "LookY";
        // Y lives on the negative source: the axis value is positive-minus-negative, so putting the
        // mouse's Y here makes "mouse moved up" the positive reading while keeping the sign of a
        // mouse count meaningful. Which world direction that turns the camera is the camera's
        // business, not the binding's.
        lookY.Negative.Kind = InputAxis::SourceKind::MouseMotion;
        lookY.Negative.Scale = -1.0f;
        lookY.Gate = InputAxis::Source { InputAxis::SourceKind::MouseButton, Mouse::ButtonRight, 1.0f };
        map.m_Axes.push_back(std::move(lookY));

        InputAxis lookGamepadX;
        lookGamepadX.Name = "GamepadLookX";
        lookGamepadX.Positive = padAxisSource(2);
        map.m_Axes.push_back(std::move(lookGamepadX));

        InputAxis lookGamepadY;
        lookGamepadY.Name = "GamepadLookY";
        lookGamepadY.Positive = padAxisSource(3);
        map.m_Axes.push_back(std::move(lookGamepadY));

        InputAxis scroll;
        scroll.Name = "Scroll";
        scroll.Positive.Kind = InputAxis::SourceKind::MouseWheel;
        map.m_Axes.push_back(std::move(scroll));

        return map;
    }

    void InputMap::Evaluate(const InputSnapshot& snapshot)
    {
        for (const InputAction& action : m_Actions)
        {
            ActionState& state = m_ActionStates[action.Name];

            // A rebind takes effect on the frame it happens, and that frame must not look like the
            // player just pressed the new key.
            if (std::find(m_ActionsNeedingReset.begin(), m_ActionsNeedingReset.end(), action.Name)
                != m_ActionsNeedingReset.end())
            {
                state = ActionState {};
            }

            EvaluateAction(action, snapshot, state);
        }

        m_ActionsNeedingReset.clear();

        for (const InputAxis& axis : m_Axes)
        {
            m_AxisValues[axis.Name] = EvaluateAxis(axis, snapshot);
        }
    }

    void InputMap::EvaluateAction(const InputAction& action, const InputSnapshot& snapshot, ActionState& state) const
    {
        state.Previous = state.Current;

        bool down = false;
        for (const InputBinding& binding : action.Bindings)
        {
            if (IsBindingDown(binding, snapshot))
            {
                down = true;
                break;
            }
        }

        state.Current = down;
    }

    float InputMap::EvaluateAxis(const InputAxis& axis, const InputSnapshot& snapshot) const
    {
        if (axis.Gate.Kind != InputAxis::SourceKind::None)
        {
            const float gate = ReadSource(axis.Gate, snapshot, axis.MouseSensitivity);
            if (gate == 0.0f)
            {
                return 0.0f;
            }
        }

        float value = ReadSource(axis.Positive, snapshot, axis.MouseSensitivity)
            - ReadSource(axis.Negative, snapshot, axis.MouseSensitivity);

        if (!axis.IsStateful())
        {
            // A delta axis passes through unclamped: 40 counts of mouse movement is a real value,
            // and truncating it to 1 would drop most of the input.
            return value;
        }

        // Only a stick needs a dead zone; a key that is down is intentional.
        const bool analog = axis.Positive.Kind == InputAxis::SourceKind::GamepadAxis
            || axis.Negative.Kind == InputAxis::SourceKind::GamepadAxis;

        if (analog)
        {
            value = ApplyDeadZone(value, axis.DeadZone);
        }

        return std::clamp(value, -1.0f, 1.0f);
    }

    float InputMap::ReadSource(const InputAxis::Source& source, const InputSnapshot& snapshot, float mouseSensitivity)
    {
        switch (source.Kind)
        {
            case InputAxis::SourceKind::Key:
                return snapshot.IsKeyPressed(source.Code) ? source.Scale : 0.0f;
            case InputAxis::SourceKind::MouseButton:
                return snapshot.IsMousePressed(source.Code) ? source.Scale : 0.0f;
            case InputAxis::SourceKind::GamepadAxis:
                return snapshot.GetGamepadAxis(source.Code) * source.Scale;
            case InputAxis::SourceKind::MouseMotion:
            {
                // The sign of Scale also selects the axis: positive reads X, negative reads Y. The
                // snapshot already carries the per-frame delta, so no frame time is needed here.
                const float counts = source.Scale >= 0.0f ? snapshot.MouseDeltaX : snapshot.MouseDeltaY;
                return counts * mouseSensitivity;
            }
            case InputAxis::SourceKind::MouseWheel:
                return snapshot.ScrollDelta;
            case InputAxis::SourceKind::None:
                return 0.0f;
        }
        return 0.0f;
    }

    bool InputMap::IsBindingDown(const InputBinding& binding, const InputSnapshot& snapshot)
    {
        switch (binding.Type)
        {
            case InputBinding::Kind::Key:
                return snapshot.IsKeyPressed(binding.Code);
            case InputBinding::Kind::MouseButton:
                return snapshot.IsMousePressed(binding.Code);
            case InputBinding::Kind::GamepadButton:
                return snapshot.IsGamepadButtonPressed(binding.Code);
        }
        return false;
    }

    bool InputMap::WarnUnknownName(const std::string& name) const
    {
        if (std::find(m_WarnedNames.begin(), m_WarnedNames.end(), name) == m_WarnedNames.end())
        {
            m_WarnedNames.push_back(name);
            HE_CORE_WARN("Input name '{}' is not in the input map", name);
        }
        return false;
    }

    bool InputMap::GetActionPressed(const std::string& name) const
    {
        const auto it = m_ActionStates.find(name);
        if (it == m_ActionStates.end())
        {
            return WarnUnknownName(name);
        }

        return it->second.Current && !it->second.Previous;
    }

    bool InputMap::GetActionHeld(const std::string& name) const
    {
        const auto it = m_ActionStates.find(name);
        if (it == m_ActionStates.end())
        {
            return WarnUnknownName(name);
        }
        return it->second.Current;
    }

    bool InputMap::GetActionReleased(const std::string& name) const
    {
        const auto it = m_ActionStates.find(name);
        if (it == m_ActionStates.end())
        {
            return WarnUnknownName(name);
        }
        return !it->second.Current && it->second.Previous;
    }

    float InputMap::GetAxis(const std::string& name) const
    {
        const auto it = m_AxisValues.find(name);
        if (it == m_AxisValues.end())
        {
            WarnUnknownName(name);
            return 0.0f;
        }
        return it->second;
    }

    bool InputMap::GetActionState(const std::string& name, bool& outCurrent, bool& outPrevious) const
    {
        const auto it = m_ActionStates.find(name);
        if (it == m_ActionStates.end())
        {
            outCurrent = false;
            outPrevious = false;
            return false;
        }

        outCurrent = it->second.Current;
        outPrevious = it->second.Previous;
        return true;
    }

    void InputMap::ResetState()
    {
        for (auto& [name, state] : m_ActionStates)
        {
            state.Current = false;
            state.Previous = false;
        }
        for (auto& [name, value] : m_AxisValues)
        {
            value = 0.0f;
        }
    }

    void InputMap::SetActionBindings(const std::string& name, const std::vector<InputBinding>& bindings)
    {
        for (InputAction& action : m_Actions)
        {
            if (action.Name == name)
            {
                action.Bindings = bindings;
                // The previous frame belongs to the old bindings; keeping it would make the next
                // frame report an edge the player never caused.
                m_ActionStates[name] = ActionState {};
                m_ActionsNeedingReset.push_back(name);
                return;
            }
        }

        m_Actions.push_back(InputAction { name, bindings });
        m_ActionStates[name] = ActionState {};
        m_ActionsNeedingReset.push_back(name);
    }

    void InputMap::SetAxis(const InputAxis& axis)
    {
        for (InputAxis& existing : m_Axes)
        {
            if (existing.Name == axis.Name)
            {
                existing = axis;
                m_AxisValues[axis.Name] = 0.0f;
                return;
            }
        }

        m_Axes.push_back(axis);
        m_AxisValues[axis.Name] = 0.0f;
    }

    std::string InputMap::SaveToText() const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << FormatVersionKey << YAML::Value << CurrentFormatVersion;

        out << YAML::Key << "Actions" << YAML::Value << YAML::BeginSeq;
        for (const InputAction& action : m_Actions)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << action.Name;
            out << YAML::Key << "Bindings" << YAML::Value << YAML::BeginSeq;
            for (const InputBinding& binding : action.Bindings)
            {
                out << YAML::Flow << YAML::BeginMap;
                out << YAML::Key << "Type" << YAML::Value << BindingKindName(binding.Type);
                out << YAML::Key << "Code" << YAML::Value << BindingCodeName(binding);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Axes" << YAML::Value << YAML::BeginSeq;
        for (const InputAxis& axis : m_Axes)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << axis.Name;

            // Named sourceKey rather than key: a parameter that shadows an outer name used on the
            // same line is how a lookup ends up reading the wrong variable.
            const auto emitSource = [&out](const char* sourceKey, const InputAxis::Source& source)
            {
                out << YAML::Key << sourceKey << YAML::Value << YAML::Flow << YAML::BeginMap;
                out << YAML::Key << "Type" << YAML::Value << SourceKindName(source.Kind);
                out << YAML::Key << "Code" << YAML::Value << SourceCodeName(source);
                out << YAML::Key << "Scale" << YAML::Value << source.Scale;
                out << YAML::EndMap;
            };

            emitSource("Positive", axis.Positive);
            emitSource("Negative", axis.Negative);
            emitSource("Gate", axis.Gate);
            out << YAML::Key << "MouseSensitivity" << YAML::Value << axis.MouseSensitivity;
            out << YAML::Key << "DeadZone" << YAML::Value << axis.DeadZone;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;
        return out.c_str();
    }

    bool InputMap::LoadFromText(const std::string& text)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(text);
        }
        catch (const YAML::Exception& exception)
        {
            HE_CORE_ERROR("Input map is not valid YAML: {}", exception.what());
            return false;
        }

        if (!data || !data.IsMap() || !data["Actions"] || !data["Axes"])
        {
            HE_CORE_ERROR("Input map is missing its Actions or Axes section");
            return false;
        }

        const int formatVersion = data[FormatVersionKey].as<int>(0);
        if (formatVersion != CurrentFormatVersion)
        {
            HE_CORE_ERROR("Input map uses format version {}; this build reads version {}",
                formatVersion,
                CurrentFormatVersion);
            return false;
        }

        // Built aside and only committed once every entry parsed: a document with one bad binding
        // must leave the caller's map alone rather than half-replace it.
        std::vector<InputAction> actions;
        std::vector<InputAxis> axes;

        for (const YAML::Node actionNode : data["Actions"])
        {
            InputAction action;
            action.Name = actionNode["Name"].as<std::string>("");
            if (action.Name.empty())
            {
                HE_CORE_ERROR("Input map contains an action without a name");
                return false;
            }

            for (const YAML::Node bindingNode : actionNode["Bindings"])
            {
                InputBinding binding;
                const std::string code = bindingNode["Code"].as<std::string>("");
                if (!InputNames::ParseBindingKind(bindingNode["Type"].as<std::string>(""), binding.Type)
                    || !ParseBindingCode(binding.Type, code, binding.Code))
                {
                    HE_CORE_ERROR("Input action '{}' has an unknown binding '{}'", action.Name, code);
                    return false;
                }

                action.Bindings.push_back(binding);
            }

            actions.push_back(std::move(action));
        }

        for (const YAML::Node axisNode : data["Axes"])
        {
            InputAxis axis;
            axis.Name = axisNode["Name"].as<std::string>("");
            if (axis.Name.empty())
            {
                HE_CORE_ERROR("Input map contains an axis without a name");
                return false;
            }

            const auto readSource = [&axisNode, &axis](const char* sourceKey, InputAxis::Source& outSource)
            {
                const YAML::Node node = axisNode[sourceKey];
                if (!node)
                {
                    return true;
                }

                const std::string code = node["Code"].as<std::string>("");
                if (!InputNames::ParseSourceKind(node["Type"].as<std::string>(""), outSource.Kind)
                    || !ParseSourceCode(outSource.Kind, code, outSource.Code))
                {
                    HE_CORE_ERROR("Input axis '{}' has an unknown source '{}'", axis.Name, code);
                    return false;
                }

                outSource.Scale = node["Scale"].as<float>(1.0f);
                return true;
            };

            if (!readSource("Positive", axis.Positive)
                || !readSource("Negative", axis.Negative)
                || !readSource("Gate", axis.Gate))
            {
                return false;
            }

            if (!axis.IsValid())
            {
                HE_CORE_ERROR("Input axis '{}' has neither a positive nor a negative source", axis.Name);
                return false;
            }

            axis.MouseSensitivity = axisNode["MouseSensitivity"].as<float>(axis.MouseSensitivity);
            axis.DeadZone = axisNode["DeadZone"].as<float>(axis.DeadZone);
            axes.push_back(std::move(axis));
        }

        m_Actions = std::move(actions);
        m_Axes = std::move(axes);
        m_ActionStates.clear();
        m_AxisValues.clear();
        m_WarnedNames.clear();
        return true;
    }
}
