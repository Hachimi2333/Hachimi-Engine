#pragma once

#include "Core/Base.h"

#include <mutex>
#include <string>
#include <vector>

namespace HachimiEngine
{
    struct EditorContext;

    // In-editor mirror of the engine and client console loggers.
    class ConsolePanel
    {
    public:
        ConsolePanel();
        ~ConsolePanel();

        void RegisterCallbacks();
        void UnregisterCallbacks();
        void Draw();

    private:
        void PushMessage(const std::string& message);

    private:
        std::mutex m_MessagesMutex;
        std::vector<std::string> m_Messages;
    };
}
