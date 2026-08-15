#include "Panels/EditorLayer.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Scene.h"

#include <imgui.h>

namespace HachimiEngine
{
    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
    {
    }

    void EditorLayer::OnAttach()
    {
        const Ref<Project> project = ProjectManager::GetActiveProject();
        if (project == nullptr)
        {
            HE_CLIENT_ERROR("Cannot open editor without an active project");
            return;
        }

        AssetManager::Init(project->GetAssetsDirectory());
        m_Scene = project->GetActiveScene();
        m_EditorCamera.SetViewportSize(Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight());

        HE_CLIENT_INFO("Editing project: {}", project->GetName());
    }

    void EditorLayer::OnDetach()
    {
        AssetManager::Shutdown();
    }

    void EditorLayer::OnUpdate(Timestep timestep)
    {
        m_EditorCamera.OnUpdate(timestep);
        if (m_Scene != nullptr)
        {
            m_Scene->OnUpdate(timestep);
        }
    }

    void EditorLayer::OnImGuiRender()
    {
        DrawDockSpace();
        DrawMenuBar();
        RenderScene();
    }

    void EditorLayer::OnEvent(Event& event)
    {
    }

    void EditorLayer::RenderScene()
    {
        if (m_Scene == nullptr)
        {
            return;
        }

        RenderCommand::SetViewport(0, 0, Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight());
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.10f, 1.0f });
        RenderCommand::Clear();
        m_Scene->OnRender(m_EditorCamera);
    }

    void EditorLayer::DrawDockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("EditorDockSpace", nullptr, windowFlags);
        ImGui::DockSpace(ImGui::GetID("EditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }

    void EditorLayer::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene"))
                {
                    const Ref<Project> project = ProjectManager::GetActiveProject();
                    if (project != nullptr)
                    {
                        project->SaveActiveScene();
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit Editor"))
                {
                    Application::Get().Close();
                }

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
}
