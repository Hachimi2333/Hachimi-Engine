#pragma once

#include "Core/Application.h"

#include <filesystem>

namespace HachimiEngine
{
    // Editor process entry application; owns editor layers created in the constructor.
    class EditorApplication final : public Application
    {
    public:
        // An empty project file opens the project hub; otherwise the editor opens that project
        // directly, which is what "--project <path.hproj>" on the command line produces.
        explicit EditorApplication(const std::filesystem::path& projectFile = {});
        ~EditorApplication() override = default;
    };
}
