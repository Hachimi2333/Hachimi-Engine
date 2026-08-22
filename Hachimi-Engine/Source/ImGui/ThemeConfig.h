#pragma once

#include <imgui.h>

namespace HachimiEngine
{
    // Centralized ImGui theme configuration for the editor UI.
    // All style values live in ThemeConfig.cpp so ImGuiLayer and panels never hardcode colors or metrics.
    class ThemeConfig final
    {
    public:
        ThemeConfig() = delete;

        // Applies the square, dark, blue-accented editor theme to an ImGui style.
        static void Apply(ImGuiStyle& style);
    };
}
