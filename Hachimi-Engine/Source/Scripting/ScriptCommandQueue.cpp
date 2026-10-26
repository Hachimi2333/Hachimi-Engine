#include "Scripting/ScriptCommandQueue.h"

#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"

namespace HachimiEngine
{
    Ref<ScriptEntityState> ScriptCommandQueue::CreateEntity(const std::string& name)
    {
        auto state = CreateRef<ScriptEntityState>();
        state->Pending = true;
        m_Commands.push_back({ Kind::Create, state, {}, name });
        return state;
    }

    void ScriptCommandQueue::DestroyEntity(Ref<ScriptEntityState> entity)
    {
        m_Commands.push_back({ Kind::Destroy, std::move(entity), {}, {} });
    }

    void ScriptCommandQueue::AddComponent(Ref<ScriptEntityState> entity, const std::string& name)
    {
        m_Commands.push_back({ Kind::Add, std::move(entity), {}, name });
    }

    void ScriptCommandQueue::RemoveComponent(Ref<ScriptEntityState> entity, const std::string& name)
    {
        m_Commands.push_back({ Kind::Remove, std::move(entity), {}, name });
    }

    void ScriptCommandQueue::SetParent(Ref<ScriptEntityState> child, Ref<ScriptEntityState> parent)
    {
        m_Commands.push_back({ Kind::Parent, std::move(child), std::move(parent), {} });
    }

    void ScriptCommandQueue::ClearParent(Ref<ScriptEntityState> child)
    {
        m_Commands.push_back({ Kind::Unparent, std::move(child), {}, {} });
    }

    void ScriptCommandQueue::Flush(Scene& scene)
    {
        // Newly queued commands belong to the next boundary, even if a component hook queues one.
        std::vector<Command> commands;
        commands.swap(m_Commands);
        for (const Command& command : commands)
        {
            if (command.Target == nullptr)
                continue;
            auto& state = *command.Target;
            if (command.Type == Kind::Create)
            {
                state.Value = scene.CreateEntity(command.Name);
                state.Pending = false;
                continue;
            }

            Entity entity = state.Value;
            if (state.Pending || !entity || entity.GetScene() != &scene)
                continue;

            switch (command.Type)
            {
                case Kind::Destroy:
                    scene.DestroyEntity(entity);
                    state.Value = {};
                    break;
                case Kind::Parent:
                    if (command.Parent != nullptr && !command.Parent->Pending && command.Parent->Value
                        && command.Parent->Value.GetScene() == &scene)
                        scene.SetParent(entity, command.Parent->Value);
                    break;
                case Kind::Unparent:
                    scene.ClearParent(entity);
                    break;
                case Kind::Add:
                case Kind::Remove:
                {
                    const ComponentDescriptor* found = nullptr;
                    for (const auto& descriptor : ComponentRegistry::GetDescriptors())
                    {
                        if (descriptor.Name == command.Name || descriptor.DisplayName == command.Name)
                        {
                            found = &descriptor;
                            break;
                        }
                    }
                    if (found == nullptr)
                    {
                        HE_CORE_WARN("Script command names an unknown component '{}'", command.Name);
                        break;
                    }
                    if (command.Type == Kind::Add && !found->Has(scene.GetRegistry(), entity.GetHandle()))
                        found->AddDefault(scene.GetRegistry(), entity.GetHandle());
                    else if (command.Type == Kind::Remove && !found->Required)
                        found->Remove(scene.GetRegistry(), entity.GetHandle());
                    break;
                }
                default: break;
            }
        }
    }

    void ScriptCommandQueue::Clear()
    {
        for (const auto& command : m_Commands)
            if (command.Type == Kind::Create && command.Target != nullptr)
                command.Target->Pending = false;
        m_Commands.clear();
    }
}
