#include "Core/Application.h"

#include "Asset/AssetDatabase.h"
#include "Asset/TextureCache.h"
#include "Core/Assert.h"
#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Core/Timestep.h"
#include "Events/ApplicationEvent.h"
#include "Events/EventDispatcher.h"
#include "ImGui/ImGuiLayer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RendererContext.h"
#include "Scripting/ScriptManager.h"
#include "Utils/VirtualFileSystem.h"

#include <chrono>

namespace HachimiEngine
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props)
    {
        HE_CORE_ASSERT(s_Instance == nullptr);
        s_Instance = this;

        // Workers must exist before any layer or renderer asks for asynchronous
        // asset loading.
        JobSystem::Init();

        m_Window = Window::Create(props);
        m_Window->SetEventCallback([this](Event& event) { OnEvent(event); });

        RenderCommand::Init();
        RenderCommand::SetClearColor({ 0.08f, 0.08f, 0.10f, 1.0f });

        m_RendererContext = CreateScope<RendererContext>();
        m_RendererContext->Init();

        // The asset services are application-wide and the renderer only borrows them, so a pass
        // can resolve an asset handle without reaching for a global.
        m_AssetDatabase = CreateScope<AssetDatabase>();
        m_TextureCache = CreateScope<TextureCache>();
        m_TextureCache->SetDatabase(m_AssetDatabase.get());
        m_RendererContext->SetAssetDatabase(m_AssetDatabase.get());
        m_RendererContext->SetTextureCache(m_TextureCache.get());

        ScriptManager::Init();

        m_ImGuiLayer = CreateRef<ImGuiLayer>();
        PushOverlay(m_ImGuiLayer);
    }

    Application::~Application()
    {
        ScriptManager::Shutdown();

        // Texture objects hold GL handles, so they are released while the context is still
        // current, and before the renderer context that created the backend they belong to.
        m_TextureCache->Clear();
        m_AssetDatabase->Clear();

        // Releases every renderer GPU resource, including the backend, while the GL
        // context is still current and before the ImGui overlay detaches.
        m_RendererContext->Shutdown();

        // Stop workers before dropping the mounts so no read is in flight while
        // packages are released, and no deferred callback can fire afterwards.
        JobSystem::Shutdown();
        VirtualFileSystem::UnmountAll();

        s_Instance = nullptr;
    }

    void Application::Run()
    {
        using Clock = std::chrono::steady_clock;

        auto lastFrameTime = Clock::now();

        while (m_Running)
        {
            const auto now = Clock::now();
            const float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;
            const Timestep timestep(deltaTime);

            // Deliver completed background reads and GPU-upload finished texture
            // decodes on the main thread, where OpenGL calls are legal.
            VirtualFileSystem::PumpCompletedRequests();
            m_TextureCache->PumpCompletedRequests();

            m_Window->OnUpdate();

            AppUpdateEvent updateEvent;
            OnEvent(updateEvent);
            m_LayerStack.Update(timestep);

            if (!m_Minimized)
            {
                AppRenderEvent renderEvent;
                OnEvent(renderEvent);

                RenderCommand::Clear();

                // Draw layer-owned framebuffers into the default framebuffer
                // before ImGui renders on top.
                m_LayerStack.Render();

                m_ImGuiLayer->Begin();
                m_LayerStack.RenderImGui();
                m_ImGuiLayer->End();

                m_Window->SwapBuffers();
            }
        }
    }

    void Application::Close()
    {
        m_Running = false;
    }

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& closeEvent) { return OnWindowClose(closeEvent); });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& resizeEvent) { return OnWindowResize(resizeEvent); });

        m_LayerStack.DispatchEvent(event);
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        Close();
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& event)
    {
        if (event.GetWidth() == 0 || event.GetHeight() == 0)
        {
            m_Minimized = true;
            return false;
        }

        m_Minimized = false;
        RenderCommand::SetViewport(0, 0, event.GetWidth(), event.GetHeight());
        return false;
    }
}
