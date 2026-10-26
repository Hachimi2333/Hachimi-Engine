#pragma once

#include "Core/Layer.h"
#include "Core/Memory.h"
#include "Packaging/PackageFormat.h"
#include "Renderer/SceneRenderTarget.h"

#include <filesystem>

namespace HachimiEngine
{
    class Project;
    class RendererContext;
    class Scene;
    class SceneRenderer;
    class ScreenPresenter;
    class SceneRuntime;

    // Runtime layer for a packaged game: loads the project straight out of the
    // game package and drives scene simulation plus rendering for the lifetime of
    // the Player.
    class PlayerLayer final : public Layer
    {
    public:
        PlayerLayer(PackageBuildInfo buildInfo, std::filesystem::path contentRoot);
        // Defined out of line: the layer owns a SceneRenderer and a ScreenPresenter, and the
        // destructor needs their complete types.
        ~PlayerLayer() override;

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(Timestep timestep) override;
        void OnRender() override;

    private:
        PackageBuildInfo m_BuildInfo;
        std::filesystem::path m_ContentRoot;
        Ref<Project> m_Project;
        Scope<SceneRuntime> m_Runtime;
        Scope<ScreenPresenter> m_ScreenPresenter;
    };
}
