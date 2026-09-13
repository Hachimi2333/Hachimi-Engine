#include "Panels/EditorShortcuts.h"

#include "Core/Log.h"
#include "Editor/CommandHistory.h"
#include "Editor/SceneCommands.h"
#include "Panels/EditorContext.h"
#include "Panels/EditorLayer.h"

#include <imgui.h>

namespace HachimiEngine
{
    void EditorShortcuts::Handle(EditorLayer& layer, EditorContext& context)
    {
        // A focused text field owns the keyboard: undo belongs to the field, not to the scene.
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const bool control = io.KeyCtrl;
        const bool shift = io.KeyShift;

        if (control && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            // Play mode edits belong to the runtime copy, so its history is the only one to rewind.
            if (context.History != nullptr && context.History->Undo())
            {
                HE_CLIENT_INFO("Undo");
            }
            return;
        }

        if (control && (ImGui::IsKeyPressed(ImGuiKey_Y, false)
                || (shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))))
        {
            if (context.History != nullptr && context.History->Redo())
            {
                HE_CLIENT_INFO("Redo");
            }
            return;
        }

        if (control && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            layer.SaveActiveScene();
            return;
        }

        if (control && !shift && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            if (context.SelectedEntity && context.ActiveScene != nullptr && context.History != nullptr)
            {
                Entity duplicate;
                if (Scope<EditorCommand> command =
                        SceneCommands::MakeDuplicateEntity(*context.ActiveScene, context.SelectedEntity, duplicate))
                {
                    // The copy already exists, so the command is recorded as an applied edit rather
                    // than executed again.
                    context.History->Execute(std::move(command));
                    context.SelectEntity(duplicate);
                }
            }
            return;
        }
    }
}
