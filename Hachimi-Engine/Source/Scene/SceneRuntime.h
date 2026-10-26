#pragma once

#include "Core/InputMap.h"
#include "Core/Memory.h"
#include "Core/Time.h"
#include "Core/Timestep.h"
#include "Renderer/RenderView.h"

namespace HachimiEngine
{
    class Scene;
    class RendererContext;
    class SceneRenderTarget;
    class DebugDraw;
    struct SceneRuntimePresentation;

    // One independent game session, shared by Editor Play and Player. Simulation is headless;
    // render resources are created lazily by the renderer implementation on the first Render.
    class SceneRuntime
    {
    public:
        SceneRuntime();
        explicit SceneRuntime(RendererContext& context, const InputMap* input = nullptr);
        ~SceneRuntime();
        SceneRuntime(const SceneRuntime&) = delete;
        SceneRuntime& operator=(const SceneRuntime&) = delete;

        bool Attach(Ref<Scene> scene);
        void Detach();
        Ref<Scene> GetScene() const { return m_Scene; }
        bool IsRunning() const;
        bool IsPaused() const { return m_Paused; }
        void SetPaused(bool paused);
        void SetInputEnabled(bool enabled);
        void SetInputMap(const InputMap& input);
        void Update(Timestep timestep);
        void Update(Timestep timestep, const InputSnapshot& snapshot);
        void Step();
        Time& GetTime() { return m_Time; }
        const InputMap& GetInputMap() const { return m_Input; }

        RenderView BuildPrimaryCameraView(float aspectRatio) const;
        static RenderView BuildPrimaryCameraView(Scene& scene, float aspectRatio);
        void Resize(uint32_t width, uint32_t height);
        void Render(const RenderView& view);
        SceneRenderTarget& GetTarget();
        DebugDraw& GetDebugDraw();

    private:
        void EnsurePresentation();
        RendererContext* m_Renderer = nullptr;
        Time m_Time;
        InputMap m_Input;
        Ref<Scene> m_Scene;
        Scope<SceneRuntimePresentation> m_Presentation;
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;
        bool m_Paused = false;
        bool m_InputEnabled = true;
    };
}
