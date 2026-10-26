#include "PlayerLayer.h"

#include "Asset/AssetDatabase.h"
#include "Asset/TextureCache.h"
#include "Core/Application.h"
#include "Core/Log.h"
#include "Core/StartupTimer.h"
#include "Project/Project.h"
#include "Renderer/MaterialResolver.h"
#include "Renderer/RendererContext.h"
#include "Renderer/ScreenPresenter.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"
#include "Serialization/ProjectSerializer.h"
#include "Serialization/SceneSerializer.h"
#include "Utils/VirtualFileSystem.h"

#include <algorithm>

namespace HachimiEngine
{
    PlayerLayer::PlayerLayer(PackageBuildInfo buildInfo, std::filesystem::path contentRoot)
        : Layer("PlayerLayer")
        , m_BuildInfo(std::move(buildInfo))
        , m_ContentRoot(std::move(contentRoot))
    {
    }

    PlayerLayer::~PlayerLayer() = default;

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

        // The asset database scans the packaged content root: it reads the sidecars that shipped
        // inside the package, so script and material references resolve exactly as they did in the
        // editor. Nothing is written back.
        AssetDatabase& assets = Application::Get().GetAssetDatabase();
        TextureCache& textures = Application::Get().GetTextureCache();
        { StartupTimer timer("Asset scan"); assets.Refresh(m_Project->GetAssetsDirectory()); }
        textures.Clear();
        textures.SetDatabase(&assets);

        m_Project->SetAssetDatabase(&assets);
        SceneSerializer::SetAssetDatabase(&assets);

        const std::filesystem::path startScenePath = m_BuildInfo.StartScene.empty()
            ? m_Project->GetStartScenePath()
            : m_ContentRoot / m_BuildInfo.StartScene;

        const auto loadScene = [&] { StartupTimer timer("Scene load"); return m_Project->OpenScene(startScenePath); };
        if (!loadScene())
        {
            HE_CLIENT_ERROR("Failed to load packaged start scene: {}", startScenePath.string());
            Application::Get().Close();
            return;
        }

        const InputMap input = InputMap::LoadProject(m_Project->GetAssetsDirectory());
        m_Runtime = CreateScope<SceneRuntime>(Application::Get().GetRendererContext(), &input);
        if (!m_Runtime->Attach(m_Project->GetActiveScene()))
        {
            Application::Get().Close();
            return;
        }
        Application::Get().GetWindow().SetCursorMode(Window::CursorMode::Disabled);

        m_ScreenPresenter = CreateScope<ScreenPresenter>();

        HE_CLIENT_INFO("Started packaged game '{}' from {}", m_Project->GetName(), m_ContentRoot.string());
    }

    void PlayerLayer::OnDetach()
    {
        Application::Get().GetWindow().SetCursorMode(Window::CursorMode::Normal);
        if (m_Runtime != nullptr) m_Runtime->Detach();
        m_Runtime.reset();
        m_ScreenPresenter.reset();
        m_Project.reset();
        Application::Get().GetRendererContext().GetMaterials().Clear();
    }

    void PlayerLayer::OnUpdate(Timestep timestep)
    {
        if (m_Runtime != nullptr) m_Runtime->Update(timestep);
    }

    void PlayerLayer::OnRender()
    {
        if (m_Runtime == nullptr || !m_Runtime->IsRunning() || m_ScreenPresenter == nullptr) return;
        const Window& window = Application::Get().GetWindow();
        const uint32_t width = std::max(window.GetWidth(), 1u);
        const uint32_t height = std::max(window.GetHeight(), 1u);
        m_Runtime->Resize(width, height);
        m_Runtime->Render(m_Runtime->BuildPrimaryCameraView(static_cast<float>(width) / static_cast<float>(height)));
        m_ScreenPresenter->Present(m_Runtime->GetTarget().GetDisplayColorRendererID(), width, height);
    }
}
