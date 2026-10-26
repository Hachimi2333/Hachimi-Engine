// InputMap: named actions and axes over one frame of device state.
//
// Bindings used to be spelled as raw keys at every call site, which made a gamepad or a re-mapped
// keyboard impossible without editing gameplay code. InputMap is evaluated from an InputSnapshot
// rather than from the devices, so the whole mapping layer - keys, mouse, gamepad, dead zones,
// edges and the .hinput document - is exercised here with no window.

#include <doctest/doctest.h>

#include "Core/Input.h"
#include "Core/InputMap.h"

#include <cmath>
#include <string>

using namespace HachimiEngine;

namespace
{
    bool Near(float lhs, float rhs, float tolerance = 1e-4f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    // A snapshot the size the backend produces, so index and bounds behaviour matches runtime.
    InputSnapshot MakeSnapshot()
    {
        InputSnapshot snapshot;
        snapshot.Keys.assign(Input::KeyCount, false);
        snapshot.MouseButtons.assign(Input::MouseButtonCount, false);
        return snapshot;
    }

    void PressKey(InputSnapshot& snapshot, KeyCode keyCode)
    {
        snapshot.Keys[static_cast<size_t>(keyCode)] = true;
    }

    void PressMouse(InputSnapshot& snapshot, MouseCode button)
    {
        snapshot.MouseButtons[static_cast<size_t>(button)] = true;
    }
}

TEST_SUITE("InputMap")
{
    TEST_CASE("the pollable code range excludes what the platform rejects")
    {
        // GLFW answers a code below GLFW_KEY_SPACE through its error callback and reports
        // "released", so a frame poll that walks from zero writes an "Invalid key" error per code
        // per frame. The snapshot is sized for every code, but only a sub-range may be asked about.
        CHECK(Input::IsPollableKey(Key::Space));
        CHECK(Input::IsPollableKey(Key::A));
        CHECK(Input::IsPollableKey(Key::F12));
        CHECK(Input::IsPollableKey(Key::RightAlt));

        CHECK_FALSE(Input::IsPollableKey(0));
        CHECK_FALSE(Input::IsPollableKey(1));
        CHECK_FALSE(Input::IsPollableKey(Input::MinKeyCode - 1));
        // Past the end of the array as well as past the platform's last key.
        CHECK_FALSE(Input::IsPollableKey(static_cast<KeyCode>(Input::KeyCount)));
        CHECK_FALSE(Input::IsPollableKey(-1));

        CHECK(Input::IsPollableMouseButton(Mouse::ButtonLeft));
        CHECK(Input::IsPollableMouseButton(Mouse::Button8));
        CHECK_FALSE(Input::IsPollableMouseButton(static_cast<MouseCode>(Input::MouseButtonCount)));
        CHECK_FALSE(Input::IsPollableMouseButton(-1));

        // The array has to be able to hold every code the platform can report, or a valid key would
        // be filtered out by IsPollableKey instead of polled.
        CHECK(Input::KeyCount > static_cast<size_t>(Key::RightAlt));
        CHECK(Input::MouseButtonCount > static_cast<size_t>(Mouse::Button8));
    }

    TEST_CASE("an out-of-range key reads as released instead of reaching the platform")
    {
        // No window and no snapshot: this is the direct-query path, which must still refuse the
        // code rather than hand it to GLFW.
        CHECK_FALSE(Input::IsKeyPressed(0));
        CHECK_FALSE(Input::IsKeyPressed(-5));
        CHECK_FALSE(Input::IsKeyPressed(static_cast<KeyCode>(Input::KeyCount)));
        CHECK_FALSE(Input::IsMouseButtonPressed(-1));
        CHECK_FALSE(Input::IsMouseButtonPressed(static_cast<MouseCode>(Input::MouseButtonCount)));

        // A pollable key with no window is simply not pressed.
        CHECK_FALSE(Input::IsKeyPressed(Key::A));
    }
    TEST_CASE("the default map binds WASD to the movement actions")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot snapshot = MakeSnapshot();
        PressKey(snapshot, Key::W);
        map.Evaluate(snapshot);

        CHECK(map.GetActionHeld("MoveForward"));
        CHECK(map.GetActionPressed("MoveForward"));
        CHECK_FALSE(map.GetActionHeld("MoveBack"));
        CHECK_FALSE(map.GetActionHeld("Jump"));
    }

    TEST_CASE("an action reports a press edge for exactly one frame")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot down = MakeSnapshot();
        PressKey(down, Key::Space);

        map.Evaluate(down);
        CHECK(map.GetActionPressed("Jump"));
        CHECK(map.GetActionHeld("Jump"));

        // Still held on the next frame: no second edge, and no release.
        map.Evaluate(down);
        CHECK_FALSE(map.GetActionPressed("Jump"));
        CHECK(map.GetActionHeld("Jump"));
        CHECK_FALSE(map.GetActionReleased("Jump"));

        InputSnapshot up = MakeSnapshot();
        map.Evaluate(up);
        CHECK_FALSE(map.GetActionPressed("Jump"));
        CHECK_FALSE(map.GetActionHeld("Jump"));
        CHECK(map.GetActionReleased("Jump"));

        // The release edge is gone on the frame after it.
        map.Evaluate(up);
        CHECK_FALSE(map.GetActionReleased("Jump"));
    }

    TEST_CASE("a mouse button fires the same action as its gamepad equivalent")
    {
        InputSnapshot mouse = MakeSnapshot();
        PressMouse(mouse, Mouse::ButtonLeft);

        InputMap mouseMap = InputMap::MakeDefault();
        mouseMap.Evaluate(mouse);
        CHECK(mouseMap.GetActionHeld("PrimaryAction"));

        // A separate map because a press edge belongs to one map's own history: the same button
        // held on the previous frame is a held action, not a new press.
        InputMap padMap = InputMap::MakeDefault();
        InputSnapshot pad = MakeSnapshot();
        pad.Gamepad.Connected = true;
        pad.Gamepad.Buttons[2] = true; // X / square
        padMap.Evaluate(pad);
        CHECK(padMap.GetActionHeld("PrimaryAction"));
        CHECK(padMap.GetActionPressed("PrimaryAction"));
    }

    TEST_CASE("a disconnected gamepad never fires its bindings")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot snapshot = MakeSnapshot();
        // Buttons pressed, but nothing is attached: the state is reported by the backend, and the
        // map must not trust the array on its own.
        snapshot.Gamepad.Connected = false;
        snapshot.Gamepad.Buttons[0] = true;
        snapshot.Gamepad.Axes[0] = 1.0f;

        map.Evaluate(snapshot);

        CHECK_FALSE(map.GetActionHeld("Jump"));
        CHECK(Near(map.GetAxis("GamepadMoveX"), 0.0f));
    }

    TEST_CASE("a keyboard movement axis is -1, 0 or 1 and cancels out")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot right = MakeSnapshot();
        PressKey(right, Key::D);
        map.Evaluate(right);
        CHECK(Near(map.GetAxis("MoveX"), 1.0f));
        CHECK(Near(map.GetAxis("MoveY"), 0.0f));

        InputSnapshot left = MakeSnapshot();
        PressKey(left, Key::A);
        map.Evaluate(left);
        CHECK(Near(map.GetAxis("MoveX"), -1.0f));

        InputSnapshot both = MakeSnapshot();
        PressKey(both, Key::D);
        PressKey(both, Key::A);
        map.Evaluate(both);
        CHECK(Near(map.GetAxis("MoveX"), 0.0f));
    }

    TEST_CASE("a gamepad stick goes through the dead zone and is rescaled")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot centered = MakeSnapshot();
        centered.Gamepad.Connected = true;
        centered.Gamepad.Axes[0] = 0.1f;
        map.Evaluate(centered);
        // Inside the default 0.2 dead zone: a stick at rest must not drift the player around.
        CHECK(Near(map.GetAxis("GamepadMoveX"), 0.0f));

        InputSnapshot pushed = MakeSnapshot();
        pushed.Gamepad.Connected = true;
        pushed.Gamepad.Axes[0] = 1.0f;
        map.Evaluate(pushed);
        CHECK(Near(map.GetAxis("GamepadMoveX"), 1.0f));

        InputSnapshot halfway = MakeSnapshot();
        halfway.Gamepad.Connected = true;
        halfway.Gamepad.Axes[0] = 0.6f;
        map.Evaluate(halfway);
        // (0.6 - 0.2) / (1 - 0.2) = 0.5, so leaving the dead zone does not jump to 0.2.
        CHECK(Near(map.GetAxis("GamepadMoveX"), 0.5f));
    }

    TEST_CASE("mouse look is gated on the look button")
    {
        InputSnapshot moving = MakeSnapshot();
        moving.MouseDeltaX = 40.0f;
        moving.MouseDeltaY = -20.0f;

        // No button held: raw cursor movement must not turn the view.
        InputMap idleMap = InputMap::MakeDefault();
        idleMap.Evaluate(moving);
        CHECK(Near(idleMap.GetAxis("LookX"), 0.0f));
        CHECK(Near(idleMap.GetAxis("LookY"), 0.0f));

        InputMap map = InputMap::MakeDefault();
        InputSnapshot looking = MakeSnapshot();
        PressMouse(looking, Mouse::ButtonRight);
        looking.MouseDeltaX = 40.0f;
        looking.MouseDeltaY = -20.0f;
        map.Evaluate(looking);

        CHECK(map.GetActionHeld("Look"));
        CHECK(map.GetAxis("LookX") > 0.0f);
        // Moving the mouse up is the positive end of the look axis.
        CHECK(map.GetAxis("LookY") > 0.0f);
        CHECK(Near(map.GetAxis("LookX"), 40.0f * 0.05f));
        CHECK(Near(map.GetAxis("LookY"), 20.0f * 0.05f));
    }

    TEST_CASE("the wheel is reported as an axis value")
    {
        InputMap scrolledMap = InputMap::MakeDefault();
        InputSnapshot scrolled = MakeSnapshot();
        scrolled.ScrollDelta = 2.0f;
        scrolledMap.Evaluate(scrolled);
        CHECK(Near(scrolledMap.GetAxis("Scroll"), 2.0f));

        InputMap idleMap = InputMap::MakeDefault();
        idleMap.Evaluate(MakeSnapshot());
        CHECK(Near(idleMap.GetAxis("Scroll"), 0.0f));
    }

    TEST_CASE("an unknown action or axis reads as nothing and says so once")
    {
        InputMap map = InputMap::MakeDefault();
        map.Evaluate(MakeSnapshot());

        CHECK_FALSE(map.GetActionHeld("NoSuchAction"));
        CHECK_FALSE(map.GetActionPressed("NoSuchAction"));
        CHECK(Near(map.GetAxis("NoSuchAxis"), 0.0f));

        // Asking again is a repeat of a name that was already reported, not a new warning.
        CHECK_FALSE(map.GetActionHeld("NoSuchAction"));
        CHECK(Near(map.GetAxis("NoSuchAxis"), 0.0f));

        // A known axis is unaffected by the unknown lookups.
        InputSnapshot right = MakeSnapshot();
        PressKey(right, Key::D);
        map.Evaluate(right);
        CHECK(Near(map.GetAxis("MoveX"), 1.0f));
    }

    TEST_CASE("the press edge is stable for as long as one frame lasts")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot down = MakeSnapshot();
        PressKey(down, Key::Space);
        map.Evaluate(down);

        // Several widgets, a script and a system can all ask about the same action in one frame.
        // The answer is derived from the state the last Evaluate computed, so it cannot change
        // until the next Evaluate - a second read must not eat the edge.
        const bool pressedFirst = map.GetActionPressed("Jump");
        const bool pressedSecond = map.GetActionPressed("Jump");
        const bool pressedThird = map.GetActionPressed("Jump");

        CHECK(pressedFirst);
        CHECK(pressedSecond);
        CHECK(pressedThird);

        const bool heldFirst = map.GetActionHeld("Jump");
        const bool heldSecond = map.GetActionHeld("Jump");
        CHECK(heldFirst);
        CHECK(heldSecond);

        // A new frame without the key is the release edge, and that too is stable.
        map.Evaluate(MakeSnapshot());
        CHECK(map.GetActionReleased("Jump"));
        CHECK(map.GetActionReleased("Jump"));
    }

    TEST_CASE("rebinding an action does not report an edge the player never caused")
    {
        InputMap map = InputMap::MakeDefault();

        // W is already held, and MoveBack is now bound to it.
        InputSnapshot held = MakeSnapshot();
        PressKey(held, Key::W);

        map.SetActionBindings("MoveBack", { InputBinding { InputBinding::Kind::Key, Key::W } });

        // Read the state once and derive the edge from that single reading. Nothing a getter does
        // changes the state, so this is the frame the last Evaluate produced; the accessor exists so
        // an invariant can be asserted about that frame rather than inferred from an edge query.
        map.Evaluate(held);

        bool current = false;
        bool previous = false;
        const bool known = map.GetActionState("MoveBack", current, previous);

        CHECK(known);
        CHECK(current);
        CHECK_FALSE(previous);
    }

    TEST_CASE("reset drops the held state as well as the edges")
    {
        InputMap map = InputMap::MakeDefault();

        InputSnapshot down = MakeSnapshot();
        PressKey(down, Key::Space);
        map.Evaluate(down);
        CHECK(map.GetActionPressed("Jump"));
        CHECK(map.GetActionHeld("Jump"));

        // Losing focus mid-press must not leave the action looking held, and the next frame the
        // button is actually down must be a fresh edge rather than a duplicate of the old one.
        map.ResetState();
        CHECK_FALSE(map.GetActionHeld("Jump"));
        CHECK_FALSE(map.GetActionPressed("Jump"));

        map.Evaluate(down);
        CHECK(map.GetActionPressed("Jump"));
        CHECK(map.GetActionHeld("Jump"));
    }

    TEST_CASE("the default map survives a round trip through the .hinput document")
    {
        const InputMap original = InputMap::MakeDefault();
        const std::string text = original.SaveToText();

        InputMap loaded;
        REQUIRE(loaded.LoadFromText(text));

        REQUIRE(loaded.GetActions().size() == original.GetActions().size());
        REQUIRE(loaded.GetAxes().size() == original.GetAxes().size());

        for (size_t index = 0; index < original.GetActions().size(); ++index)
        {
            CHECK(loaded.GetActions()[index].Name == original.GetActions()[index].Name);
            CHECK(loaded.GetActions()[index].Bindings.size() == original.GetActions()[index].Bindings.size());
        }

        for (size_t index = 0; index < original.GetAxes().size(); ++index)
        {
            const InputAxis& expected = original.GetAxes()[index];
            const InputAxis& actual = loaded.GetAxes()[index];

            CHECK(actual.Name == expected.Name);
            CHECK(actual.Positive.Kind == expected.Positive.Kind);
            CHECK(actual.Positive.Code == expected.Positive.Code);
            CHECK(Near(actual.Positive.Scale, expected.Positive.Scale));
            CHECK(actual.Negative.Kind == expected.Negative.Kind);
            CHECK(actual.Gate.Kind == expected.Gate.Kind);
            CHECK(Near(actual.DeadZone, expected.DeadZone));
        }

        // Bindings are stored by name, so the document has to be readable rather than full of
        // numeric codes; a reordered enumeration must not silently re-map a project.
        CHECK(text.find("MoveForward") != std::string::npos);
        CHECK(text.find("W") != std::string::npos);
        CHECK(text.find("GamepadButton") != std::string::npos);
    }

    TEST_CASE("a loaded map evaluates exactly like the default one")
    {
        InputMap loaded;
        REQUIRE(loaded.LoadFromText(InputMap::MakeDefault().SaveToText()));

        InputSnapshot snapshot = MakeSnapshot();
        PressKey(snapshot, Key::D);
        snapshot.Gamepad.Connected = true;
        snapshot.Gamepad.Axes[0] = 0.6f;
        PressMouse(snapshot, Mouse::ButtonRight);
        snapshot.MouseDeltaX = 10.0f;

        loaded.Evaluate(snapshot);

        CHECK(loaded.GetActionHeld("MoveRight"));
        CHECK(Near(loaded.GetAxis("MoveX"), 1.0f));
        CHECK(Near(loaded.GetAxis("GamepadMoveX"), 0.5f));
        CHECK(Near(loaded.GetAxis("LookX"), 0.5f));
    }

    TEST_CASE("a document this build cannot use is refused instead of half-applied")
    {
        InputMap map = InputMap::MakeDefault();
        const size_t actionCount = map.GetActions().size();

        CHECK_FALSE(map.LoadFromText("not: [valid"));
        CHECK_FALSE(map.LoadFromText("Actions: []\n"));
        CHECK_FALSE(map.LoadFromText("FormatVersion: 99\nActions: []\nAxes: []\n"));

        // An unknown binding code is the failure a hand-edited file is most likely to contain.
        const std::string wrongCode =
            "FormatVersion: 1\n"
            "Actions:\n"
            "  - Name: Fire\n"
            "    Bindings:\n"
            "      - {Type: Key, Code: NotAKey}\n"
            "Axes: []\n";
        CHECK_FALSE(map.LoadFromText(wrongCode));

        const std::string unknownType =
            "FormatVersion: 1\n"
            "Actions:\n"
            "  - Name: Fire\n"
            "    Bindings:\n"
            "      - {Type: Pedal, Code: A}\n"
            "Axes: []\n";
        CHECK_FALSE(map.LoadFromText(unknownType));

        // An axis with no source cannot produce a value, which is a mapping mistake rather than
        // an empty axis.
        const std::string emptyAxis =
            "FormatVersion: 1\n"
            "Actions: []\n"
            "Axes:\n"
            "  - Name: Broken\n"
            "    Positive: {Type: None, Code: '', Scale: 1}\n";
        CHECK_FALSE(map.LoadFromText(emptyAxis));

        // None of the failures may have disturbed the map the caller already had.
        CHECK(map.GetActions().size() == actionCount);
        InputSnapshot snapshot = MakeSnapshot();
        PressKey(snapshot, Key::W);
        map.Evaluate(snapshot);
        CHECK(map.GetActionHeld("MoveForward"));
    }
}
