#include "Panels/ToolbarPanel.h"

#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"

#include <ImGuizmo.h>
#include <imgui.h>

namespace HachimiEngine
{
    void ToolbarPanel::Draw(EditorLayer* owner, EditorContext& context)
    {
        ImGui::Begin("Toolbar");

        ImGui::TextUnformatted("Gizmo:");

        ImGui::SameLine();
        if (ImGui::Button("Translate"))
        {
            context.GizmoOperation = ImGuizmo::TRANSLATE;
        }

        ImGui::SameLine();
        if (ImGui::Button("Rotate"))
        {
            context.GizmoOperation = ImGuizmo::ROTATE;
        }

        ImGui::SameLine();
        if (ImGui::Button("Scale"))
        {
            context.GizmoOperation = ImGuizmo::SCALE;
        }

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextUnformatted("Playback:");

        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState == EditorPlayState::Playing);
        if (ImGui::Button("Play"))
        {
            owner->OnPlay();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState == EditorPlayState::Stopped);
        if (ImGui::Button("Pause"))
        {
            owner->OnPause();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(context.PlayState == EditorPlayState::Stopped);
        if (ImGui::Button("Stop"))
        {
            owner->OnStop();
        }
        ImGui::EndDisabled();

        ImGui::End();
    }
}
