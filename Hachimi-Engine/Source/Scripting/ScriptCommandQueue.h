#pragma once

#include "Core/Memory.h"
#include "Scene/Entity.h"

#include <string>
#include <vector>

namespace HachimiEngine
{
    class Scene;

    // Shared by copies of a script handle. Creation resolves it only at the frame boundary.
    struct ScriptEntityState
    {
        Entity Value;
        bool Pending = false;
        mutable bool Warned = false;
    };

    // Structural scene edits are data until Scene flushes them after Update, before LateUpdate.
    // Commands resolve live handles in order, so operations after a destroy are safely discarded.
    class ScriptCommandQueue
    {
    public:
        Ref<ScriptEntityState> CreateEntity(const std::string& name);
        void DestroyEntity(Ref<ScriptEntityState> entity);
        void AddComponent(Ref<ScriptEntityState> entity, const std::string& name);
        void RemoveComponent(Ref<ScriptEntityState> entity, const std::string& name);
        void SetParent(Ref<ScriptEntityState> child, Ref<ScriptEntityState> parent);
        void ClearParent(Ref<ScriptEntityState> child);
        void Flush(Scene& scene);
        void Clear();
        bool IsEmpty() const { return m_Commands.empty(); }

    private:
        enum class Kind { Create, Destroy, Add, Remove, Parent, Unparent };
        struct Command
        {
            Kind Type;
            Ref<ScriptEntityState> Target;
            Ref<ScriptEntityState> Parent;
            std::string Name;
        };
        std::vector<Command> m_Commands;
    };
}
