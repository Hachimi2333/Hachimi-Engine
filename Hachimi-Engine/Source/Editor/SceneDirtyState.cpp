#include "Editor/SceneDirtyState.h"

#include "Scene/Scene.h"

namespace HachimiEngine
{
    void SceneDirtyState::Bind(const Ref<Scene>& scene)
    {
        m_Scene = scene;
        m_LastKnownDirty = IsDirty();
        ++m_Revision;
    }

    bool SceneDirtyState::IsDirty() const
    {
        const bool dirty = m_Scene != nullptr && m_Scene->IsDirty();

        // The serializer clears the scene's own flag after a successful save, which the editor
        // never sees as a call through here. Refreshing on read keeps the revision honest without
        // making the editor poll for it.
        if (dirty != m_LastKnownDirty)
        {
            m_LastKnownDirty = dirty;
            ++m_Revision;
        }

        return dirty;
    }

    void SceneDirtyState::MarkDirty()
    {
        if (m_Scene != nullptr)
        {
            m_Scene->MarkDirty();
        }

        if (!m_LastKnownDirty)
        {
            m_LastKnownDirty = true;
            ++m_Revision;
        }
    }

    void SceneDirtyState::ClearDirty()
    {
        if (m_Scene != nullptr)
        {
            m_Scene->ClearDirty();
        }

        if (m_LastKnownDirty)
        {
            m_LastKnownDirty = false;
            ++m_Revision;
        }
    }
}
