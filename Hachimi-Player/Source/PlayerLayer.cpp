#include "PlayerLayer.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Project/Project.h"
#include "Renderer/PostProcessPass.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Serialization/ProjectSerializer.h"
#include "Utils/FileSystem.h"
#include "Utils/VirtualFileSystem.h"
#include "Math/Math.h"

#include <glad/gl.h>

#include <algorithm>

namespace HachimiEngine
{
    namespace
    {
        constexpr uint32_t DefaultWindowWidth = 1600;
        constexpr uint32_t DefaultWindowHeight = 900;

        void ResizeFramebufferIfNeeded(const Ref<Framebuffer>& framebuffer, uint32_t width, uint32_t height)
        {
            if (framebuffer == nullptr)
            {
                return;
            }

            const auto& specification = framebuffer->GetSpecification();
            if (width > 0 && height > 0 && (width != specification.Width || height != specification.Height))
            {
                framebuffer->Resize(width, height);
            }
        }
    }

    PlayerLayer::PlayerLayer(PackageBuildInfo buildInfo, std::filesystem::path contentRoot)
        : Layer("PlayerLayer")
        , m_BuildInfo(std::move(buildInfo))
        , m_ContentRoot(std::move(contentRoot))
    {
        FramebufferSpecification sceneSpecification;
        sceneSpecification.Width = DefaultWindowWidth;
        sceneSpecification.Height = DefaultWindowHeight;
        sceneSpecification.ColorFormat = FramebufferColorFormat::RGBA16F;
        m_SceneFramebuffer = Framebuffer::Create(sceneSpecification);
    }

    void PlayerLayer::OnAttach()
    {
        if (m_ContentRoot.empty() || !VirtualFileSystem::IsArchiveMounted())
        {
            HE_CLIENT_ERROR("No game package is mounted; closing the player");
            Application::Get().Close();
            return;
        }

        // Everything below resolves through the virtual file system, so these
        // reads come straight out of Data.hpak instead of an extracted copy.
        const std::filesystem::path projectFilePath = m_ContentRoot / "Project.hproj";
        if (!VirtualFileSystem::Exists(projectFilePath))
        {
            HE_CLIENT_ERROR("Packaged project file does not exist: {}", projectFilePath.string());
            Application::Get().Close();
            return;
        }

        m_Project = CreateRef<Project>();
        ProjectSerializer serializer(m_Project);
        if (!serializer.Deserialize(projectFilePath.string()))
        {
            HE_CLIENT_ERROR("Failed to load packaged project file: {}", projectFilePath.string());
            Application::Get().Close();
            return;
        }

        AssetManager::Init(m_Project->GetAssetsDirectory());

        const std::filesystem::path startScenePath = m_BuildInfo.StartScene.empty()
            ? m_Project->GetStartScenePath()
            : m_ContentRoot / m_BuildInfo.StartScene;

        if (!m_Project->OpenScene(startScenePath))
        {
            HE_CLIENT_ERROR("Failed to load packaged start scene: {}", startScenePath.string());
            AssetManager::Shutdown();
            Application::Get().Close();
            return;
        }

        m_Scene = m_Project->GetActiveScene();

        const Window& window = Application::Get().GetWindow();
        m_Scene->SetViewportSize(window.GetWidth(), window.GetHeight());
        m_Scene->OnRuntimeStart();

        HE_CLIENT_INFO("Started packaged game '{}' from {}", m_Project->GetName(), m_ContentRoot.string());
    }

    void PlayerLayer::OnDetach()
    {
        if (m_Scene != nullptr)
        {
            m_Scene->OnRuntimeStop();
        }

        // Release the scene and its texture resources before the asset manager
        // shuts down.
        m_Scene = nullptr;
        m_Project = nullptr;

        AssetManager::Shutdown();
    }

    void PlayerLayer::OnUpdate(Timestep timestep)
    {
        if (m_Scene == nullptr)
        {
            return;
        }

        m_Scene->OnUpdate(timestep);

        const Window& window = Application::Get().GetWindow();
        const uint32_t width = std::max(window.GetWidth(), 1u);
        const uint32_t height = std::max(window.GetHeight(), 1u);
        ResizeFramebufferIfNeeded(m_SceneFramebuffer, width, height);
        m_Scene->SetViewportSize(width, height);

        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        Math::Mat4 viewMatrix(1.0f);
        Math::Mat4 projectionMatrix(1.0f);
        Math::Vec3 cameraPosition(0.0f);

        const Entity primaryCamera = m_Scene->GetPrimaryCameraEntity();
        if (primaryCamera
            && primaryCamera.HasComponent<TransformComponent>()
            && primaryCamera.HasComponent<CameraComponent>())
        {
            const auto& cameraComponent = primaryCamera.GetComponent<CameraComponent>();
            const Math::Mat4 cameraWorld = m_Scene->GetWorldTransform(primaryCamera.GetHandle());
            viewMatrix = Math::Inverse(cameraWorld);
            cameraPosition = Math::Vec3(cameraWorld[3].x, cameraWorld[3].y, cameraWorld[3].z);
            projectionMatrix = Math::Perspective(
                Math::Radians(cameraComponent.FieldOfView),
                aspectRatio,
                cameraComponent.NearClip,
                cameraComponent.FarClip);
        }
        else
        {
            static bool warnedOnce = false;
            if (!warnedOnce)
            {
                HE_CORE_WARN("Packaged scene has no primary camera; using a fallback camera");
                warnedOnce = true;
            }
            cameraPosition = { 0.0f, 6.0f, 12.0f };
            viewMatrix = Math::LookAt(cameraPosition, Math::Vec3(0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
            projectionMatrix = Math::Perspective(Math::Radians(45.0f), aspectRatio, 0.1f, 1000.0f);
        }

        m_SceneFramebuffer->Bind();
        RenderCommand::SetClearColor({ 0.00719f, 0.00719f, 0.01002f, 1.0f });
        RenderCommand::Clear();
        m_Scene->OnRender(viewMatrix, projectionMatrix, cameraPosition);
        m_SceneFramebuffer->Unbind();
    }

    void PlayerLayer::OnRender()
    {
        if (m_Scene == nullptr || m_SceneFramebuffer->GetColorAttachmentRendererID() == 0)
        {
            return;
        }

        const Window& window = Application::Get().GetWindow();
        RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
        PostProcessPass::Render(m_SceneFramebuffer->GetColorAttachmentRendererID());
    }
}
