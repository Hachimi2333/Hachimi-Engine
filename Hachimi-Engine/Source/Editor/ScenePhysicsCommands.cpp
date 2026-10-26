#include "Editor/ScenePhysicsCommands.h"

#include "Physics/PhysicsSettings.h"
#include "Scene/Scene.h"

#include <string>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        // One field of the scene's physics settings.
        //
        // Every field behaves the same way - write one value on apply, the other on revert, and fold
        // a repeated edit of the same field into this entry - so the field itself is the only thing a
        // subclass has to name. That keeps a new setting down to a factory function.
        template<typename TValue>
        class PhysicsSettingCommand final : public EditorCommand
        {
        public:
            using Member = TValue PhysicsSettings::*;

            PhysicsSettingCommand(std::string name, Member field, const TValue& before, const TValue& after)
                : m_Name(std::move(name))
                , m_Field(field)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return m_Name; }

            void Apply(Scene& scene) override { scene.GetPhysicsSettings().*m_Field = m_After; }
            void Revert(Scene& scene) override { scene.GetPhysicsSettings().*m_Field = m_Before; }

            // A slider or a drag reports a new value every frame it moves, so dragging gravity from
            // one value to another is one undo entry rather than one per frame.
            bool TryMerge(const EditorCommand& next) override
            {
                const auto* other = dynamic_cast<const PhysicsSettingCommand*>(&next);
                if (other == nullptr || other->m_Field != m_Field)
                {
                    return false;
                }

                m_After = other->m_After;
                return true;
            }

        private:
            std::string m_Name;
            Member m_Field = nullptr;
            TValue m_Before {};
            TValue m_After {};
        };
    }

    namespace ScenePhysicsCommands
    {
        Scope<EditorCommand> MakeSetGravity(const Math::Vec3& before, const Math::Vec3& after)
        {
            return CreateScope<PhysicsSettingCommand<Math::Vec3>>(
                "Change Gravity", &PhysicsSettings::Gravity, before, after);
        }

        Scope<EditorCommand> MakeSetFixedTimeStep(float before, float after)
        {
            return CreateScope<PhysicsSettingCommand<float>>(
                "Change Fixed Time Step", &PhysicsSettings::FixedTimeStep, before, after);
        }

        Scope<EditorCommand> MakeSetSubStepCount(int before, int after)
        {
            return CreateScope<PhysicsSettingCommand<int>>(
                "Change Sub Steps", &PhysicsSettings::SubStepCount, before, after);
        }

        Scope<EditorCommand> MakeSetEnableSleep(bool before, bool after)
        {
            return CreateScope<PhysicsSettingCommand<bool>>(
                "Toggle Sleeping", &PhysicsSettings::EnableSleep, before, after);
        }

        Scope<EditorCommand> MakeSetEnableContinuous(bool before, bool after)
        {
            return CreateScope<PhysicsSettingCommand<bool>>(
                "Toggle Continuous Collision", &PhysicsSettings::EnableContinuous, before, after);
        }

        Scope<EditorCommand> MakeSetEnableContactEvents(bool before, bool after)
        {
            return CreateScope<PhysicsSettingCommand<bool>>(
                "Toggle Contact Events", &PhysicsSettings::EnableContactEvents, before, after);
        }

        Scope<EditorCommand> MakeSetWorkerCount(uint32_t before, uint32_t after)
        {
            return CreateScope<PhysicsSettingCommand<uint32_t>>(
                "Change Physics Workers", &PhysicsSettings::WorkerCount, before, after);
        }
    }
}
