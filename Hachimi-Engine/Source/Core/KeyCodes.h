#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    // Key codes intentionally mirror GLFW values so the GLFW backend can forward them directly.
    using KeyCode = int;

    namespace Key
    {
        inline constexpr KeyCode Space = 32;
        inline constexpr KeyCode Apostrophe = 39;
        inline constexpr KeyCode Comma = 44;
        inline constexpr KeyCode Minus = 45;
        inline constexpr KeyCode Period = 46;
        inline constexpr KeyCode Slash = 47;
        inline constexpr KeyCode D0 = 48;
        inline constexpr KeyCode D1 = 49;
        inline constexpr KeyCode D2 = 50;
        inline constexpr KeyCode D3 = 51;
        inline constexpr KeyCode D4 = 52;
        inline constexpr KeyCode D5 = 53;
        inline constexpr KeyCode D6 = 54;
        inline constexpr KeyCode D7 = 55;
        inline constexpr KeyCode D8 = 56;
        inline constexpr KeyCode D9 = 57;
        inline constexpr KeyCode Semicolon = 59;
        inline constexpr KeyCode Equal = 61;
        inline constexpr KeyCode A = 65;
        inline constexpr KeyCode B = 66;
        inline constexpr KeyCode C = 67;
        inline constexpr KeyCode D = 68;
        inline constexpr KeyCode E = 69;
        inline constexpr KeyCode F = 70;
        inline constexpr KeyCode G = 71;
        inline constexpr KeyCode H = 72;
        inline constexpr KeyCode I = 73;
        inline constexpr KeyCode J = 74;
        inline constexpr KeyCode K = 75;
        inline constexpr KeyCode L = 76;
        inline constexpr KeyCode M = 77;
        inline constexpr KeyCode N = 78;
        inline constexpr KeyCode O = 79;
        inline constexpr KeyCode P = 80;
        inline constexpr KeyCode Q = 81;
        inline constexpr KeyCode R = 82;
        inline constexpr KeyCode S = 83;
        inline constexpr KeyCode T = 84;
        inline constexpr KeyCode U = 85;
        inline constexpr KeyCode V = 86;
        inline constexpr KeyCode W = 87;
        inline constexpr KeyCode X = 88;
        inline constexpr KeyCode Y = 89;
        inline constexpr KeyCode Z = 90;
        inline constexpr KeyCode LeftBracket = 91;
        inline constexpr KeyCode Backslash = 92;
        inline constexpr KeyCode RightBracket = 93;
        inline constexpr KeyCode GraveAccent = 96;
        inline constexpr KeyCode Escape = 256;
        inline constexpr KeyCode Enter = 257;
        inline constexpr KeyCode Tab = 258;
        inline constexpr KeyCode Backspace = 259;
        inline constexpr KeyCode Insert = 260;
        inline constexpr KeyCode Delete = 261;
        inline constexpr KeyCode Right = 262;
        inline constexpr KeyCode Left = 263;
        inline constexpr KeyCode Down = 264;
        inline constexpr KeyCode Up = 265;
        inline constexpr KeyCode PageUp = 266;
        inline constexpr KeyCode PageDown = 267;
        inline constexpr KeyCode Home = 268;
        inline constexpr KeyCode End = 269;
        inline constexpr KeyCode CapsLock = 280;
        inline constexpr KeyCode ScrollLock = 281;
        inline constexpr KeyCode NumLock = 282;
        inline constexpr KeyCode PrintScreen = 283;
        inline constexpr KeyCode Pause = 284;
        inline constexpr KeyCode F1 = 290;
        inline constexpr KeyCode F2 = 291;
        inline constexpr KeyCode F3 = 292;
        inline constexpr KeyCode F4 = 293;
        inline constexpr KeyCode F5 = 294;
        inline constexpr KeyCode F6 = 295;
        inline constexpr KeyCode F7 = 296;
        inline constexpr KeyCode F8 = 297;
        inline constexpr KeyCode F9 = 298;
        inline constexpr KeyCode F10 = 299;
        inline constexpr KeyCode F11 = 300;
        inline constexpr KeyCode F12 = 301;
        inline constexpr KeyCode LeftShift = 340;
        inline constexpr KeyCode LeftControl = 341;
        inline constexpr KeyCode LeftAlt = 342;
        inline constexpr KeyCode RightShift = 344;
        inline constexpr KeyCode RightControl = 345;
        inline constexpr KeyCode RightAlt = 346;
    }
}
