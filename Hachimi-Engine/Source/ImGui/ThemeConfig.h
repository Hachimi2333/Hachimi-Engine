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

        // Semantic colors a panel may push for a specific widget state, so a palette change
        // reaches every panel instead of the ones that happened to be edited.
        struct SemanticColors
        {
            // Remove and delete buttons: transparent at rest, red on hover.
            ImVec4 DestructiveButton;
            ImVec4 DestructiveButtonHovered;
            ImVec4 DestructiveButtonActive;
            // Icon color used on those buttons.
            ImVec4 DestructiveButtonText;

            ImVec4 ErrorText;
            ImVec4 WarningText;
            ImVec4 SuccessText;
        };

        static const SemanticColors& GetColors();

        // Applies the square, dark, blue-accented editor theme to an ImGui style.
        static void Apply(ImGuiStyle& style);
    };
}
