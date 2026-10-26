#include "Editor/SceneEnvironmentCommands.h"

#include "Renderer/EnvironmentSettings.h"
#include "Scene/Scene.h"

#include <string>
#include <utility>

namespace HachimiEngine
{
    namespace
    {
        // One field of the scene's environment settings.
        //
        // Every field behaves the same way - write one value on apply, the other on revert, and fold
        // a repeated edit of the same field into this entry - so the field itself is the only thing a
        // subclass has to name. That keeps a new setting down to a factory function.
        template<typename TValue>
        class EnvironmentSettingCommand final : public EditorCommand
        {
        public:
            using Member = TValue EnvironmentSettings::*;

            EnvironmentSettingCommand(std::string name, Member field, const TValue& before, const TValue& after)
                : m_Name(std::move(name))
                , m_Field(field)
                , m_Before(before)
                , m_After(after)
            {
            }

            std::string_view GetName() const override { return m_Name; }

            void Apply(Scene& scene) override { scene.GetEnvironmentSettings().*m_Field = m_After; }
            void Revert(Scene& scene) override { scene.GetEnvironmentSettings().*m_Field = m_Before; }

            // A slider or a drag reports a new value every frame it moves, so dragging exposure from
            // one value to another is one undo entry rather than one per frame.
            bool TryMerge(const EditorCommand& next) override
            {
                const auto* other = dynamic_cast<const EnvironmentSettingCommand*>(&next);
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

    namespace SceneEnvironmentCommands
    {
        Scope<EditorCommand> MakeSetSkybox(bool before, bool after)
        { return CreateScope<EnvironmentSettingCommand<bool>>("Show Skybox", &EnvironmentSettings::ShowSkybox, before, after); }
        Scope<EditorCommand> MakeSetExposure(float before, float after)
        { return CreateScope<EnvironmentSettingCommand<float>>("Exposure", &EnvironmentSettings::Exposure, before, after); }
        Scope<EditorCommand> MakeSetIntensity(float before, float after)
        { return CreateScope<EnvironmentSettingCommand<float>>("Environment Intensity", &EnvironmentSettings::EnvironmentIntensity, before, after); }
    }
}
