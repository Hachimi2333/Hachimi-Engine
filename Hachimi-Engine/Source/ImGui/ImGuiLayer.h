#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"

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
    };
}
