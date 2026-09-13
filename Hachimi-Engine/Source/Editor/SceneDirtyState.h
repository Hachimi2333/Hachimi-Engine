#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstdint>

namespace HachimiEngine
{
    class Scene;

    // Tracks whether the scene the editor is working on has unsaved changes.
    //
    // The dirty flag itself lives on the scene, because that is what the serializer clears and
    // what any caller can consult. This wrapper exists for the editor's benefit: it follows the
    // active scene across Play/Stop and scene switches, and bumps a revision counter so UI that
    // shows the state (a window title, a menu entry) can refresh on change instead of comparing
    // strings every frame.
    class SceneDirtyState
    {
    public:
        void Bind(const Ref<Scene>& scene);
        const Ref<Scene>& GetScene() const { return m_Scene; }

        bool IsDirty() const;
        void MarkDirty();
        void ClearDirty();

        // Incremented by every transition, clean to dirty and back.
        uint64_t GetRevision() const { return m_Revision; }

    private:
        Ref<Scene> m_Scene;
        // Mutable because reading the state is also what notices a change made elsewhere, such as
        // the serializer clearing the flag after a save.
        mutable uint64_t m_Revision = 0;
        mutable bool m_LastKnownDirty = false;
    };
}
