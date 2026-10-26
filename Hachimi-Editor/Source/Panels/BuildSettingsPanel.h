#pragma once

#include "Core/Base.h"
#include "Editor/ProjectSettingsDraft.h"
#include "Packaging/PackageWriter.h"

#include <filesystem>
#include <string>
#include <vector>

namespace HachimiEngine
{
    struct EditorContext;
    class EditorLayer;

    // Project settings opened from the Settings or Build menu. Edits the active project's
    // per-platform export settings and invokes ProjectPackager on demand.
    class BuildSettingsPanel
    {
    public:
        void Open();
        void Draw(EditorLayer* owner, EditorContext& context);

    private:
        bool m_Open = false;
        ProjectSettingsDraft m_Draft;
        std::vector<std::filesystem::path> m_SceneOptions;
        char m_ProductName[128] = {};
        std::string m_StatusMessage;
        bool m_StatusIsError = false;
        std::filesystem::path m_LastOutputDirectory;

        // Statistics of the most recent successful export.
        bool m_HasReport = false;
        PackageBuildReport m_Report;
    };
}
