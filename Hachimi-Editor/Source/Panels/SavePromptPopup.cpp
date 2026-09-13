#include "Panels/SavePromptPopup.h"

#include <imgui.h>

namespace HachimiEngine
{
    SavePromptPopup::Choice SavePromptPopup::Draw()
    {
        if (!m_Open)
        {
            return Choice::None;
        }

        constexpr const char* PopupId = "Unsaved Changes###SavePrompt";

        if (!ImGui::IsPopupOpen(PopupId))
        {
            ImGui::OpenPopup(PopupId);
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->GetCenter().x, viewport->GetCenter().y),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);

        Choice choice = Choice::None;

        if (ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextUnformatted("The scene has unsaved changes.");
            ImGui::Spacing();
            ImGui::TextWrapped("Saving is required before: %s", m_ActionLabel.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const ImGuiStyle& style = ImGui::GetStyle();
            const float buttonWidth = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2.0f) / 3.0f;

            if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
            {
                choice = Choice::Save;
            }

            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(buttonWidth, 0.0f)))
            {
                choice = Choice::Discard;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f)))
            {
                choice = Choice::Cancel;
            }

            if (choice != Choice::None)
            {
                m_Open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        else
        {
            // The popup could not open this frame (another modal is up); try again next frame.
            m_Open = true;
        }

        return choice;
    }
}
