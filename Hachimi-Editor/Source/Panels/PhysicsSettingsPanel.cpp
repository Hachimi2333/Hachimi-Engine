#include "Panels/PhysicsSettingsPanel.h"

#include "Components/InspectorWidgets.h"
#include "Core/JobSystem.h"
#include "Editor/CommandHistory.h"
#include "Editor/ScenePhysicsCommands.h"
#include "Panels/EditorContext.h"
#include "Physics/PhysicsTaskScheduler.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Scene.h"
#include "Scene/Systems/PhysicsSystem.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        PhysicsWorld* WorldOf(Scene& scene)
        {
            PhysicsSystem* physics = scene.FindSystem<PhysicsSystem>();
            return physics != nullptr ? physics->GetWorld() : nullptr;
        }

        // Which value a control edits: the undoable scene copy, or the running one.
        //
        // Playing edits the live scene without a history entry, because that scene is thrown away when
        // play stops - there is nothing to revert to - and the whole point of editing it is to see the
        // change on the next step. The history marks the edited scene dirty itself, so only the
        // decision to record at all belongs here.
        struct EditTarget
        {
            CommandHistory* History = nullptr;
            bool Playing = false;

            void Record(Scope<EditorCommand> command) const
            {
                if (Playing)
                {
                    // The value is already written into the scene by the caller; a command would only
                    // apply it to the scene being edited, which is not the one on screen.
                    return;
                }

                RecordEdit(History, std::move(command));
            }
        };
    }

    void PhysicsSettingsPanel::Draw(EditorContext& context)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (!ImGui::Begin("Physics"))
        {
            ImGui::End();
            return;
        }

        if (context.ActiveScene == nullptr)
        {
            ImGui::TextDisabled("No scene is open.");
            ImGui::End();
            return;
        }

        PhysicsSettings& settings = context.ActiveScene->GetPhysicsSettings();
        const EditTarget target { context.History, context.IsPlaying() };

        if (target.Playing)
        {
            ImGui::TextDisabled("Edits apply to this play session only.");
        }

        if (BeginInspectorTable("PhysicsSettingsRows"))
        {
            BeginInspectorProperty("Gravity");
            {
                Math::Vec3 gravity = settings.Gravity;
                if (ImGui::DragFloat3("##Gravity", Math::ValuePtr(gravity), 0.1f))
                {
                    const Math::Vec3 before = settings.Gravity;
                    settings.Gravity = gravity;
                    target.Record(ScenePhysicsCommands::MakeSetGravity(before, gravity));
                }
            }

            BeginInspectorProperty("Fixed Time Step");
            {
                float fixedStep = settings.FixedTimeStep;
                if (ImGui::DragFloat("##FixedTimeStep", &fixedStep, 0.0005f, 0.001f, 0.1f, "%.4f s"))
                {
                    const float before = settings.FixedTimeStep;
                    settings.FixedTimeStep = fixedStep;
                    target.Record(ScenePhysicsCommands::MakeSetFixedTimeStep(before, fixedStep));
                }
            }

            BeginInspectorProperty("Sub Steps");
            {
                int subSteps = settings.SubStepCount;
                if (ImGui::SliderInt("##SubSteps", &subSteps, 1, 16))
                {
                    const int before = settings.SubStepCount;
                    settings.SubStepCount = subSteps;
                    target.Record(ScenePhysicsCommands::MakeSetSubStepCount(before, subSteps));
                }
            }

            BeginInspectorProperty("Workers");
            {
                // 0 is "as many as the pool has", which is the only way to ask for the machine's
                // whole pool without knowing it here.
                int workers = static_cast<int>(settings.WorkerCount);
                if (ImGui::SliderInt("##Workers", &workers, 0, static_cast<int>(PhysicsTaskScheduler::MaxWorkers)))
                {
                    const uint32_t before = settings.WorkerCount;
                    const uint32_t after = static_cast<uint32_t>(std::max(workers, 0));
                    settings.WorkerCount = after;
                    target.Record(ScenePhysicsCommands::MakeSetWorkerCount(before, after));
                }

                ImGui::SameLine();
                if (settings.WorkerCount == 0)
                {
                    ImGui::TextDisabled("(whole pool: %u)", JobSystem::GetWorkerCount());
                }
                else
                {
                    ImGui::TextDisabled("(pool has %u)", JobSystem::GetWorkerCount());
                }
            }

            BeginInspectorProperty("Enable Sleep");
            {
                bool enableSleep = settings.EnableSleep;
                if (ImGui::Checkbox("##EnableSleep", &enableSleep))
                {
                    settings.EnableSleep = enableSleep;
                    target.Record(ScenePhysicsCommands::MakeSetEnableSleep(!enableSleep, enableSleep));
                }
            }

            BeginInspectorProperty("Continuous");
            {
                bool enableContinuous = settings.EnableContinuous;
                if (ImGui::Checkbox("##EnableContinuous", &enableContinuous))
                {
                    settings.EnableContinuous = enableContinuous;
                    target.Record(ScenePhysicsCommands::MakeSetEnableContinuous(!enableContinuous, enableContinuous));
                }
            }

            BeginInspectorProperty("Contact Events");
            {
                bool enableContactEvents = settings.EnableContactEvents;
                if (ImGui::Checkbox("##EnableContactEvents", &enableContactEvents))
                {
                    settings.EnableContactEvents = enableContactEvents;
                    target.Record(ScenePhysicsCommands::MakeSetEnableContactEvents(!enableContactEvents, enableContactEvents));
                }
            }

            ImGui::EndTable();
        }

        PhysicsWorld* world = WorldOf(*context.ActiveScene);
        if (world == nullptr)
        {
            ImGui::End();
            return;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Runtime");

        const PhysicsTaskStats stats = world->GetTaskStats();
        ImGui::Text("Workers in use: %u", stats.WorkerCount);

        if (stats.UsesThreadPool)
        {
            ImGui::Text("Pooled tasks: %zu", stats.PoolTaskCount);
        }
        else
        {
            ImGui::Text("Tasks run inline: %zu", stats.InlineTaskCount);
        }

        if (stats.RefusedStepCount > 0)
        {
            // Refused means a step was asked for from a job, which the scheduler turns down instead of
            // deadlocking on it; showing the count is how that stays visible.
            ImGui::Text("Refused steps: %zu", stats.RefusedStepCount);
        }

        ImGui::End();
    }
}
