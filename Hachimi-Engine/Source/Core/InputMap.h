#pragma once

#include "Core/Base.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace HachimiEngine
{
    // Highest gamepad button / axis index the engine models. They mirror the GLFW gamepad
    // enumerations, so the backend copies its state across without a translation table.
    inline constexpr int GamepadButtonCount = 15;
    inline constexpr int GamepadAxisCount = 6;

    // Raw device state for one frame.
    //
    // InputMap evaluates bindings against a snapshot rather than reading the devices itself, which
    // is what lets an input mapping be exercised in a headless test: everything is one plain value
    // that a test can fill in by hand.
    struct GamepadSnapshot
    {
        bool Connected = false;
        // Indexed by the GLFW gamepad button enumeration (A/B/X/Y, bumpers, sticks, dpad, guide).
        bool Buttons[GamepadButtonCount] = {};
        // Indexed by the GLFW gamepad axis enumeration (LX/LY/RX/RY/LT/RT), already calibrated.
        float Axes[GamepadAxisCount] = {};
    };

    struct InputSnapshot
    {
        // Indexed by KeyCode / MouseCode; an unknown code reads as not pressed.
        std::vector<bool> Keys;
        std::vector<bool> MouseButtons;

        // Position in window pixels, as reported by the window backend.
        float MouseX = 0.0f;
        float MouseY = 0.0f;

        // Movement since the previous frame, and wheel notches this frame. Mouse-look bindings
        // need a delta, so the map keeps these apart from the absolute position.
        float MouseDeltaX = 0.0f;
        float MouseDeltaY = 0.0f;
        float ScrollDelta = 0.0f;

        GamepadSnapshot Gamepad;

        bool IsKeyPressed(KeyCode keyCode) const
        {
            return keyCode >= 0 && static_cast<size_t>(keyCode) < Keys.size() && Keys[static_cast<size_t>(keyCode)];
        }

        bool IsMousePressed(MouseCode button) const
        {
            return button >= 0 && static_cast<size_t>(button) < MouseButtons.size()
                && MouseButtons[static_cast<size_t>(button)];
        }

        bool IsGamepadButtonPressed(int button) const
        {
            return Gamepad.Connected && button >= 0 && button < GamepadButtonCount && Gamepad.Buttons[button];
        }

        float GetGamepadAxis(int axis) const
        {
            return Gamepad.Connected && axis >= 0 && axis < GamepadAxisCount ? Gamepad.Axes[axis] : 0.0f;
        }
    };

    // One thing a player can press.
    struct InputBinding
    {
        enum class Kind
        {
            Key = 0,
            MouseButton,
            GamepadButton
        };

        Kind Type = Kind::Key;
        // KeyCode, MouseCode or gamepad button index, depending on Type.
        int Code = 0;
    };

    struct InputAction
    {
        std::string Name;
        // Any of these being held fires the action.
        std::vector<InputBinding> Bindings;
    };

    // One analog channel built out of a positive and a negative source.
    struct InputAxis
    {
        enum class SourceKind
        {
            None = 0,
            Key,
            MouseButton,
            GamepadAxis,
            // Relative mouse movement. Which mouse axis a source reads is the sign of its Scale:
            // positive reads X, negative reads Y. That is why the code carries nothing for it.
            MouseMotion,
            MouseWheel
        };

        struct Source
        {
            SourceKind Kind = SourceKind::None;
            // KeyCode, MouseCode or gamepad axis index, depending on Kind.
            int Code = 0;
            // Weight and direction this source contributes with.
            float Scale = 1.0f;
        };

        std::string Name;
        Source Positive;
        Source Negative;
        // When set, the axis only produces a value while this source is held. Mouse-look needs it:
        // without a gate, a right-mouse-hold look axis would keep turning the camera from raw
        // cursor movement whenever the user is not holding the button.
        Source Gate;
        // Degrees of rotation per mouse count for MouseMotion sources. 0.05 is roughly a 800 DPI
        // mouse at a comfortable sensitivity; a project overrides it in .hinput.
        float MouseSensitivity = 0.05f;
        // Analog sources at or below this magnitude read as zero.
        float DeadZone = 0.2f;

        bool IsValid() const { return Positive.Kind != SourceKind::None || Negative.Kind != SourceKind::None; }

        // True for an axis made of button-like sources, which is the family that produces -1/0/1 or
        // a stick reading. Mouse movement and the wheel are per-frame deltas instead: clamping a
        // fast flick to 1 would delete the input rather than normalize it.
        bool IsStateful() const
        {
            const auto stateful = [](SourceKind kind)
            {
                return kind == SourceKind::Key || kind == SourceKind::MouseButton
                    || kind == SourceKind::GamepadAxis;
            };
            return stateful(Positive.Kind) || stateful(Negative.Kind);
        }
    };

    // Named action/axis layer over the raw Input state.
    //
    // Gameplay code asks for "MoveForward" and "LookX", never for Key::W or a GLFW button: the
    // bindings live in Assets/Input.hinput, so a project can be re-mapped - including onto a
    // gamepad - without touching a line of script or engine code.
    class InputMap
    {
    public:
        // A playable default: WASD + mouse look + space to jump + left stick on a gamepad.
        // Also what a loaded .hinput that cannot be read falls back to.
        static InputMap MakeDefault();
        static InputMap LoadProject(const std::filesystem::path& assetsDirectory);

        // Loads a map from YAML text. Returns false and leaves the map untouched when the document
        // is not a usable input map - a half-read binding table is worse than the default one.
        bool LoadFromText(const std::string& text);
        // Serializes in the same shape LoadFromText reads.
        std::string SaveToText() const;

        // Recomputes every action and axis against one frame of device state. Call once per frame,
        // before anything reads the actions. Taken by reference: a snapshot carries two button
        // arrays and the caller already owns one for the frame.
        void Evaluate(const InputSnapshot& snapshot = InputSnapshot());

        // Clears edge history. Call when input focus is lost, so re-focusing does not look like a
        // burst of new presses.
        void ResetState();

        bool GetActionPressed(const std::string& name) const;
        bool GetActionHeld(const std::string& name) const;
        bool GetActionReleased(const std::string& name) const;

        // Analog value in [-1, 1]. Unknown axes read as 0 and warn once.
        float GetAxis(const std::string& name) const;

        // Raw edge state of one action: what the last Evaluate computed. Reading it does not change
        // it, so a test can assert the invariant directly instead of inferring it from an edge
        // query. An input remapping UI uses it to show what the game is currently seeing.
        bool GetActionState(const std::string& name, bool& outCurrent, bool& outPrevious) const;

        const std::vector<InputAction>& GetActions() const { return m_Actions; }
        const std::vector<InputAxis>& GetAxes() const { return m_Axes; }

        // Editor/runtime mapping UI support. Replacing bindings resets the edge state, because the
        // "previous frame" of a binding that just changed would otherwise fire a phantom edge.
        void SetActionBindings(const std::string& name, const std::vector<InputBinding>& bindings);
        void SetAxis(const InputAxis& axis);

    private:
        struct ActionState
        {
            bool Current = false;
            bool Previous = false;
        };

        void EvaluateAction(const InputAction& action, const InputSnapshot& snapshot, ActionState& state) const;
        float EvaluateAxis(const InputAxis& axis, const InputSnapshot& snapshot) const;
        static bool IsBindingDown(const InputBinding& binding, const InputSnapshot& snapshot);
        // mouseSensitivity comes from the owning axis; a source itself only carries a weight.
        static float ReadSource(const InputAxis::Source& source, const InputSnapshot& snapshot, float mouseSensitivity);
        // Reports an unknown action or axis name once and answers "not pressed".
        bool WarnUnknownName(const std::string& name) const;

        std::vector<InputAction> m_Actions;
        std::vector<InputAxis> m_Axes;

        // Keyed by name so a missing entry is an unknown action rather than a stale one. Edge state
        // is per action, so an action that stops existing leaves nothing behind.
        std::unordered_map<std::string, ActionState> m_ActionStates;
        std::unordered_map<std::string, float> m_AxisValues;

        // Actions whose bindings changed since the last Evaluate. Their first evaluation after the
        // change establishes the new state instead of reporting an edge the player never caused.
        std::vector<std::string> m_ActionsNeedingReset;

        // One warning per name, so a missing action in a per-frame script does not flood the log.
        mutable std::vector<std::string> m_WarnedNames;
    };

    // Name conversion used by the .hinput document. Codes are stored by name, never as integers,
    // so reordering an enumeration cannot silently re-map a project. The two enumerations that are
    // written as literals in the document ("Key", "MouseButton", ...) only need a parser.
    //
    // The printer functions carry a distinct name each rather than overloading one: KeyCode and
    // MouseCode are both aliases of int, so overloads on them would be the same signature.
    namespace InputNames
    {
        const char* KeyName(KeyCode keyCode);
        const char* MouseName(MouseCode button);
        const char* GamepadButtonName(int gamepadButton);
        const char* GamepadAxisName(int gamepadAxis);

        bool ParseKey(const std::string& name, KeyCode& outKeyCode);
        bool ParseMouseButton(const std::string& name, MouseCode& outButton);
        bool ParseGamepadButton(const std::string& name, int& outButton);
        bool ParseGamepadAxis(const std::string& name, int& outAxis);
        bool ParseBindingKind(const std::string& name, InputBinding::Kind& outKind);
        bool ParseSourceKind(const std::string& name, InputAxis::SourceKind& outKind);
    }
}
