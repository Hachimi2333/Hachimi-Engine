#pragma once

#include "Core/Base.h"

namespace HachimiEngine
{
    // Mouse button codes intentionally mirror GLFW values.
    using MouseCode = int;

    namespace Mouse
    {
        inline constexpr MouseCode Button1 = 0;
        inline constexpr MouseCode Button2 = 1;
        inline constexpr MouseCode Button3 = 2;
        inline constexpr MouseCode Button4 = 3;
        inline constexpr MouseCode Button5 = 4;
        inline constexpr MouseCode Button6 = 5;
        inline constexpr MouseCode Button7 = 6;
        inline constexpr MouseCode Button8 = 7;
        inline constexpr MouseCode ButtonLeft = Button1;
        inline constexpr MouseCode ButtonRight = Button2;
        inline constexpr MouseCode ButtonMiddle = Button3;
    }
}
