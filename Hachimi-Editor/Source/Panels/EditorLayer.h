#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"
#include "Renderer/EditorCamera.h"

namespace HachimiEngine
{
    class Scene;

    // Main editor layer. Full panel layout is built on top of this layer.
    class EditorLayer final : public Layer
    {
    public:
        EditorLayer();
        ~EditorLayer() override = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(Timestep timestep) override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

    private:
        void RenderScene();
        void DrawDockSpace();
        void DrawMenuBar();

    private:
        Ref<Scene> m_Scene;
        EditorCamera m_EditorCamera;
    };
}
