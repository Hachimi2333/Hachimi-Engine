#include "ImGui/ThemeConfig.h"

#include <array>

namespace HachimiEngine
{
    namespace
    {
        // Palette ----------------------------------------------------------------------
        // Modern blue accent used for selections, active controls and focus indicators.
        constexpr ImVec4 Accent{ 0.298f, 0.553f, 1.000f, 1.000f };
        constexpr ImVec4 AccentHovered{ 0.431f, 0.639f, 1.000f, 1.000f };
        constexpr ImVec4 AccentActive{ 0.180f, 0.373f, 0.753f, 1.000f };
        constexpr ImVec4 AccentDimmed{ 0.184f, 0.369f, 0.620f, 1.000f };

        constexpr ImVec4 TextPrimary{ 0.910f, 0.922f, 0.933f, 1.000f };
        constexpr ImVec4 TextDisabled{ 0.439f, 0.475f, 0.525f, 1.000f };

        constexpr ImVec4 WindowBackground{ 0.075f, 0.082f, 0.098f, 1.000f };
        constexpr ImVec4 ChildBackground{ 0.086f, 0.094f, 0.114f, 1.000f };
        constexpr ImVec4 PopupBackground{ 0.106f, 0.118f, 0.141f, 1.000f };
        constexpr ImVec4 TitleBackground{ 0.063f, 0.075f, 0.086f, 1.000f };
        constexpr ImVec4 TitleBackgroundActive{ 0.090f, 0.106f, 0.133f, 1.000f };
        constexpr ImVec4 MenuBarBackground{ 0.063f, 0.075f, 0.086f, 1.000f };

        constexpr ImVec4 Border{ 0.149f, 0.173f, 0.212f, 1.000f };
        constexpr ImVec4 BorderStrong{ 0.188f, 0.220f, 0.275f, 1.000f };
        constexpr ImVec4 BorderLight{ 0.137f, 0.161f, 0.200f, 1.000f };

        constexpr ImVec4 FrameBackground{ 0.110f, 0.129f, 0.157f, 1.000f };
        constexpr ImVec4 FrameBackgroundHovered{ 0.141f, 0.169f, 0.208f, 1.000f };
        constexpr ImVec4 FrameBackgroundActive{ 0.173f, 0.208f, 0.259f, 1.000f };

        constexpr ImVec4 Button{ 0.118f, 0.141f, 0.173f, 1.000f };
        constexpr ImVec4 ButtonHovered{ 0.161f, 0.196f, 0.255f, 1.000f };
        constexpr ImVec4 Header{ 0.125f, 0.149f, 0.188f, 1.000f };
        constexpr ImVec4 HeaderHovered{ 0.161f, 0.196f, 0.255f, 1.000f };

        constexpr ImVec4 ScrollbarGrab{ 0.200f, 0.231f, 0.282f, 1.000f };
        constexpr ImVec4 ScrollbarGrabHovered{ 0.282f, 0.337f, 0.431f, 1.000f };
        constexpr ImVec4 Separator{ 0.149f, 0.173f, 0.212f, 1.000f };
        constexpr ImVec4 SeparatorHovered{ 0.227f, 0.271f, 0.341f, 1.000f };

        constexpr ImVec4 Tab{ 0.090f, 0.106f, 0.133f, 1.000f };
        constexpr ImVec4 TabHovered{ 0.133f, 0.157f, 0.204f, 1.000f };
        constexpr ImVec4 TabSelected{ 0.125f, 0.145f, 0.180f, 1.000f };
        constexpr ImVec4 TabDimmed{ 0.078f, 0.090f, 0.114f, 1.000f };
        constexpr ImVec4 TabDimmedSelected{ 0.114f, 0.133f, 0.165f, 1.000f };

        constexpr ImVec4 TableHeaderBackground{ 0.102f, 0.122f, 0.153f, 1.000f };
        constexpr ImVec4 TreeLines{ 0.235f, 0.275f, 0.329f, 1.000f };
        constexpr ImVec4 CheckMark{ 0.949f, 0.961f, 0.973f, 1.000f };
        constexpr ImVec4 UnsavedMarker{ 1.000f, 0.761f, 0.278f, 1.000f };

        constexpr ImVec4 Black{ 0.000f, 0.000f, 0.000f, 1.000f };
        constexpr ImVec4 White{ 1.000f, 1.000f, 1.000f, 1.000f };

        struct ColorEntry
        {
            ImGuiCol Index;
            ImVec4 Value;
        };

        // Full color table, one entry per ImGuiCol_. Keep this list in enum order and update it whenever the
        // ImGui version introduces new color slots; the static_assert below catches count mismatches.
        constexpr std::array ColorTable = {
            ColorEntry{ ImGuiCol_Text, TextPrimary },
            ColorEntry{ ImGuiCol_TextDisabled, TextDisabled },
            ColorEntry{ ImGuiCol_WindowBg, WindowBackground },
            ColorEntry{ ImGuiCol_ChildBg, ChildBackground },
            ColorEntry{ ImGuiCol_PopupBg, PopupBackground },
            ColorEntry{ ImGuiCol_Border, Border },
            ColorEntry{ ImGuiCol_BorderShadow, ImVec4{ 0.0f, 0.0f, 0.0f, 0.45f } },
            ColorEntry{ ImGuiCol_FrameBg, FrameBackground },
            ColorEntry{ ImGuiCol_FrameBgHovered, FrameBackgroundHovered },
            ColorEntry{ ImGuiCol_FrameBgActive, FrameBackgroundActive },
            ColorEntry{ ImGuiCol_TitleBg, TitleBackground },
            ColorEntry{ ImGuiCol_TitleBgActive, TitleBackgroundActive },
            ColorEntry{ ImGuiCol_TitleBgCollapsed, TitleBackground },
            ColorEntry{ ImGuiCol_MenuBarBg, MenuBarBackground },
            ColorEntry{ ImGuiCol_ScrollbarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.35f } },
            ColorEntry{ ImGuiCol_ScrollbarGrab, ScrollbarGrab },
            ColorEntry{ ImGuiCol_ScrollbarGrabHovered, ScrollbarGrabHovered },
            ColorEntry{ ImGuiCol_ScrollbarGrabActive, Accent },
            ColorEntry{ ImGuiCol_CheckMark, CheckMark },
            ColorEntry{ ImGuiCol_CheckboxSelectedBg, Accent },
            ColorEntry{ ImGuiCol_SliderGrab, Accent },
            ColorEntry{ ImGuiCol_SliderGrabActive, AccentHovered },
            ColorEntry{ ImGuiCol_Button, Button },
            ColorEntry{ ImGuiCol_ButtonHovered, ButtonHovered },
            ColorEntry{ ImGuiCol_ButtonActive, AccentActive },
            ColorEntry{ ImGuiCol_Header, Header },
            ColorEntry{ ImGuiCol_HeaderHovered, HeaderHovered },
            ColorEntry{ ImGuiCol_HeaderActive, AccentActive },
            ColorEntry{ ImGuiCol_Separator, Separator },
            ColorEntry{ ImGuiCol_SeparatorHovered, SeparatorHovered },
            ColorEntry{ ImGuiCol_SeparatorActive, Accent },
            ColorEntry{ ImGuiCol_ResizeGrip, ScrollbarGrab },
            ColorEntry{ ImGuiCol_ResizeGripHovered, ScrollbarGrabHovered },
            ColorEntry{ ImGuiCol_ResizeGripActive, AccentHovered },
            ColorEntry{ ImGuiCol_InputTextCursor, Accent },
            ColorEntry{ ImGuiCol_TabHovered, TabHovered },
            ColorEntry{ ImGuiCol_Tab, Tab },
            ColorEntry{ ImGuiCol_TabSelected, TabSelected },
            ColorEntry{ ImGuiCol_TabSelectedOverline, Accent },
            ColorEntry{ ImGuiCol_TabDimmed, TabDimmed },
            ColorEntry{ ImGuiCol_TabDimmedSelected, TabDimmedSelected },
            ColorEntry{ ImGuiCol_TabDimmedSelectedOverline, AccentDimmed },
            ColorEntry{ ImGuiCol_DockingPreview, ImVec4{ Accent.x, Accent.y, Accent.z, 0.65f } },
            ColorEntry{ ImGuiCol_DockingEmptyBg, WindowBackground },
            ColorEntry{ ImGuiCol_PlotLines, Accent },
            ColorEntry{ ImGuiCol_PlotLinesHovered, AccentHovered },
            ColorEntry{ ImGuiCol_PlotHistogram, Accent },
            ColorEntry{ ImGuiCol_PlotHistogramHovered, AccentHovered },
            ColorEntry{ ImGuiCol_TableHeaderBg, TableHeaderBackground },
            ColorEntry{ ImGuiCol_TableBorderStrong, BorderStrong },
            ColorEntry{ ImGuiCol_TableBorderLight, BorderLight },
            ColorEntry{ ImGuiCol_TableRowBg, ImVec4{ Black.x, Black.y, Black.z, 0.0f } },
            ColorEntry{ ImGuiCol_TableRowBgAlt, ImVec4{ White.x, White.y, White.z, 0.03f } },
            ColorEntry{ ImGuiCol_TextLink, Accent },
            ColorEntry{ ImGuiCol_TextSelectedBg, ImVec4{ Accent.x, Accent.y, Accent.z, 0.42f } },
            ColorEntry{ ImGuiCol_TreeLines, TreeLines },
            ColorEntry{ ImGuiCol_DragDropTarget, Accent },
            ColorEntry{ ImGuiCol_DragDropTargetBg, ImVec4{ Accent.x, Accent.y, Accent.z, 0.22f } },
            ColorEntry{ ImGuiCol_UnsavedMarker, UnsavedMarker },
            ColorEntry{ ImGuiCol_NavCursor, Accent },
            ColorEntry{ ImGuiCol_NavWindowingHighlight, Accent },
            ColorEntry{ ImGuiCol_NavWindowingDimBg, ImVec4{ Black.x, Black.y, Black.z, 0.55f } },
            ColorEntry{ ImGuiCol_ModalWindowDimBg, ImVec4{ Black.x, Black.y, Black.z, 0.60f } },
        };

        static_assert(ColorTable.size() == ImGuiCol_COUNT, "ThemeConfig color table must cover every ImGuiCol_ entry");

        void ApplyColors(ImGuiStyle& style)
        {
            for (const ColorEntry& entry : ColorTable)
            {
                style.Colors[entry.Index] = entry.Value;
            }
        }

        void ApplyMetrics(ImGuiStyle& style)
        {
            style.Alpha = 1.0f;
            style.DisabledAlpha = 0.55f;

            // Spacing and padding.
            style.WindowPadding = ImVec2{ 8.0f, 8.0f };
            style.FramePadding = ImVec2{ 6.0f, 4.0f };
            style.ItemSpacing = ImVec2{ 8.0f, 5.0f };
            style.ItemInnerSpacing = ImVec2{ 6.0f, 4.0f };
            style.CellPadding = ImVec2{ 4.0f, 3.0f };
            style.TouchExtraPadding = ImVec2{ 0.0f, 0.0f };
            style.IndentSpacing = 20.0f;
            style.ColumnsMinSpacing = 6.0f;

            // Window, child and popup geometry. Every rounding value is zero for a square theme.
            style.WindowRounding = 0.0f;
            style.WindowBorderSize = 1.0f;
            style.WindowBorderHoverPadding = 4.0f;
            style.WindowMinSize = ImVec2{ 32.0f, 32.0f };
            style.WindowTitleAlign = ImVec2{ 0.0f, 0.5f };
            style.WindowMenuButtonPosition = ImGuiDir_Left;
            style.ChildRounding = 0.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupRounding = 0.0f;
            style.PopupBorderSize = 1.0f;

            // Frames (inputs, sliders, checkboxes, buttons).
            style.FrameRounding = 0.0f;
            style.FrameBorderSize = 0.0f;

            // Scrollbars and sliders.
            style.ScrollbarSize = 14.0f;
            style.ScrollbarRounding = 0.0f;
            style.ScrollbarPadding = 0.0f;
            style.GrabMinSize = 10.0f;
            style.GrabRounding = 0.0f;
            style.LogSliderDeadzone = 4.0f;

            // Images and tabs.
            style.ImageRounding = 0.0f;
            style.ImageBorderSize = 0.0f;
            style.TabRounding = 0.0f;
            style.TabBorderSize = 0.0f;
            style.TabMinWidthBase = 1.0f;
            style.TabMinWidthShrink = 80.0f;
            style.TabCloseButtonMinWidthSelected = -1.0f;
            style.TabCloseButtonMinWidthUnselected = 0.0f;
            style.TabBarBorderSize = 1.0f;
            style.TabBarOverlineSize = 2.0f;

            // Trees, menus and selectables.
            style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesNone;
            style.TreeLinesSize = 1.0f;
            style.TreeLinesRounding = 0.0f;
            style.MenuItemRounding = 0.0f;
            style.SelectableRounding = 0.0f;

            // Drag and drop targets.
            style.DragDropTargetRounding = 0.0f;
            style.DragDropTargetBorderSize = 2.0f;
            style.DragDropTargetPadding = 3.0f;

            // Widget decoration details.
            style.ColorMarkerSize = 3.0f;
            style.ColorButtonPosition = ImGuiDir_Right;
            style.ButtonTextAlign = ImVec2{ 0.5f, 0.5f };
            style.SelectableTextAlign = ImVec2{ 0.0f, 0.0f };
            style.InputTextCursorSize = 1.0f;
            style.SeparatorSize = 1.0f;
            style.SeparatorTextBorderSize = 2.0f;
            style.SeparatorTextAlign = ImVec2{ 0.0f, 0.5f };
            style.SeparatorTextPadding = ImVec2{ 20.0f, 3.0f };

            // Display and docking.
            style.DisplayWindowPadding = ImVec2{ 19.0f, 19.0f };
            style.DisplaySafeAreaPadding = ImVec2{ 3.0f, 3.0f };
            style.DockingNodeHasCloseButton = true;
            style.DockingSeparatorSize = 2.0f;

            // Rendering quality knobs, kept at ImGui defaults for crisp square shapes.
            style.MouseCursorScale = 1.0f;
            style.AntiAliasedLines = true;
            style.AntiAliasedLinesUseTex = true;
            style.AntiAliasedFill = true;
            style.CurveTessellationTol = 1.25f;
            style.CircleTessellationMaxError = 0.30f;
        }
    }

    void ThemeConfig::Apply(ImGuiStyle& style)
    {
        ApplyColors(style);
        ApplyMetrics(style);
    }
}
