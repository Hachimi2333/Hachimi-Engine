#include "Scene/SceneRuntime.h"

#include "Core/Input.h"
#include "Core/Log.h"
#include "Renderer/SceneRuntimePresentation.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Scene.h"

#include <algorithm>

namespace HachimiEngine
{
    SceneRuntime::SceneRuntime() : m_Input(InputMap::MakeDefault()) {}

    SceneRuntime::SceneRuntime(RendererContext& context, const InputMap* input)
        : SceneRuntime()
    {
        m_Renderer = &context;
        if (input != nullptr) SetInputMap(*input);
    }

    SceneRuntime::~SceneRuntime() { Detach(); }

    bool SceneRuntime::Attach(Ref<Scene> scene)
    {
        if (scene == nullptr) return false;
        if (m_Scene == scene && IsRunning()) return true;
        if (scene->IsRuntimeRunning())
        {
            HE_CORE_ERROR("Cannot attach a scene already owned by another runtime");
            return false;
        }
        Detach();
        m_Time.Reset();
        m_Time.SetTimeScale(1.0f);
        m_Time.SetFixedDeltaTime(scene->GetPhysicsSettings().FixedTimeStep);
        m_Input.ResetState();
        m_Scene = std::move(scene);
        m_Scene->SetInputMap(&m_Input);
        m_Scene->OnRuntimeStart(m_Time);
        m_Paused = false;
        return true;
    }

    void SceneRuntime::Detach()
    {
        if (m_Scene != nullptr)
        {
            m_Scene->OnRuntimeStop();
            m_Scene->SetInputMap(nullptr);
        }
        m_Scene.reset();
        m_Presentation.reset();
        m_Time.ClearSteps();
        m_Input.ResetState();
        m_Paused = false;
    }

    bool SceneRuntime::IsRunning() const { return m_Scene != nullptr && m_Scene->IsRuntimeRunning(); }
    void SceneRuntime::SetPaused(bool paused) { m_Paused = paused; }
    void SceneRuntime::SetInputEnabled(bool enabled)
    {
        if (m_InputEnabled != enabled) m_Input.ResetState();
        m_InputEnabled = enabled;
    }
    void SceneRuntime::SetInputMap(const InputMap& input) { m_Input = input; m_Input.ResetState(); }
    void SceneRuntime::Update(Timestep timestep) { Update(timestep, Input::GetSnapshot()); }

    void SceneRuntime::Update(Timestep timestep, const InputSnapshot& snapshot)
    {
        if (!IsRunning() || m_Paused) return;
        m_Time.SetFixedDeltaTime(m_Scene->GetPhysicsSettings().FixedTimeStep);
        m_Time.Advance(timestep.GetSeconds());
        m_Input.Evaluate(m_InputEnabled ? snapshot : InputSnapshot {});
        m_Scene->OnUpdate(Timestep(m_Time.GetUnscaledDeltaTime()));
    }

    void SceneRuntime::Step()
    {
        if (!IsRunning() || !m_Paused) return;
        const float scale = m_Time.GetTimeScale();
        m_Time.SetTimeScale(1.0f);
        m_Time.Advance(m_Scene->GetPhysicsSettings().FixedTimeStep);
        m_Time.SetTimeScale(scale);
        m_Input.Evaluate(InputSnapshot {});
        m_Scene->Step();
    }

    RenderView SceneRuntime::BuildPrimaryCameraView(float aspectRatio) const
    {
        return m_Scene != nullptr ? BuildPrimaryCameraView(*m_Scene, aspectRatio) : RenderView {};
    }

    RenderView SceneRuntime::BuildPrimaryCameraView(Scene& scene, float aspectRatio)
    {
        SceneRenderDesc desc;
        aspectRatio = std::max(aspectRatio, 0.001f);
        Entity camera = scene.GetPrimaryCameraEntity();
        if (camera)
        {
            const auto& settings = camera.GetComponent<CameraComponent>();
            const auto world = scene.GetWorldTransform(camera.GetHandle());
            desc.View = Math::Inverse(world);
            desc.CameraPosition = Math::Vec3(world[3]);
            desc.Projection = Math::Perspective(Math::Radians(settings.FieldOfView), aspectRatio, settings.NearClip, settings.FarClip);
        }
        else
        {
            desc.CameraPosition = { 0.0f, 6.0f, 12.0f };
            desc.View = Math::LookAt(desc.CameraPosition, Math::Vec3(0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
            desc.Projection = Math::Perspective(Math::Radians(45.0f), aspectRatio, 0.1f, 1000.0f);
        }
        return scene.BuildRenderView(desc);
    }
}
