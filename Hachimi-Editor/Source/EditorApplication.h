#pragma once

#include "Core/Application.h"

namespace HachimiEngine
{
    // Editor process entry application; owns editor layers created in the constructor.
    class EditorApplication final : public Application
    {
    public:
        EditorApplication();
        ~EditorApplication() override = default;
    };
}
