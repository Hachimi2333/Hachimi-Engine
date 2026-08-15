#pragma once

#include "Core/Layer.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    // Startup screen: create a project, open a project, or reopen a recent one.
    class ProjectHubLayer final : public Layer
    {
    public:
        ProjectHubLayer();

        void OnAttach() override;
        void OnImGuiRender() override;

    private:
        void OpenProject(const std::filesystem::path& projectFilePath);

    private:
        char m_ProjectName[128] = "NewProject";
        char m_ProjectLocation[1024] = {};
        bool m_EditorPushed = false;
        Layer* m_EditorLayer = nullptr;
    };
}
