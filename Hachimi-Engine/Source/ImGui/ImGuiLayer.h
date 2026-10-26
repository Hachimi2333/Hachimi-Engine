#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"
#include "Editor/EditorSettings.h"
#include <chrono>

namespace HachimiEngine
{
    // ImGui overlay layer. Owns the docking context and the GLFW/OpenGL3 backends.
    class ImGuiLayer final : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer() override = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& event) override;

        void Begin();
        void End();
        EditorSettings& GetSettings() { return m_Settings; }

    private:
        void SaveSettings();
        EditorSettings m_Settings;
        std::chrono::steady_clock::time_point m_SaveRequestedAt {};
        bool m_SavePending = false;
    };
}
