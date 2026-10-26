#include "Panels/ToolbarPanel.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace HachimiEngine
{
    void ToolbarPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        const auto& style = ImGui::GetStyle();
        const float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
        if (!ImGui::BeginViewportSideBar("EditorToolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        { ImGui::End(); return; }

        const auto tool = [&](const char* label, ImGuizmo::OPERATION operation)
        {
            const ImVec2 size(ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f, ImGui::GetFrameHeight());
            if (ImGui::Selectable(label, context.GizmoOperation == operation, 0, size)) context.GizmoOperation = operation;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s selected objects", label);
            ImGui::SameLine();
        };
        tool("Move", ImGuizmo::TRANSLATE);
        tool("Rotate", ImGuizmo::ROTATE);
        tool("Scale", ImGuizmo::SCALE);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState == EditorPlayState::Playing);
        if (ImGui::Button(context.PlayState == EditorPlayState::Paused ? "Resume" : "Play")) owner->OnPlay();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState != EditorPlayState::Playing);
        if (ImGui::Button("Pause")) owner->OnPause();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState == EditorPlayState::Stopped);
        if (ImGui::Button("Stop")) owner->OnStop();
        ImGui::EndDisabled();
        if (context.PlayState == EditorPlayState::Paused)
        {
            ImGui::SameLine();
            if (ImGui::Button("Step")) owner->OnStep();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Advance one fixed simulation step");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", context.PlayState == EditorPlayState::Stopped ? "Editing"
            : context.PlayState == EditorPlayState::Playing ? "Playing" : "Paused");
        ImGui::End();
    }
}
