#pragma once

#include "Core/Base.h"

#include <filesystem>

namespace HachimiEngine
{
    struct EditorContext;

    // File browser rooted at the project Assets directory.
    class ContentBrowserPanel
    {
    public:
        void Draw(EditorContext& context);

    private:
        std::filesystem::path m_CurrentDirectory;
    };
}
