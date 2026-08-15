#include "Panels/ConsolePanel.h"

#include "Core/Log.h"

#include <imgui.h>

namespace HachimiEngine
{
    ConsolePanel::ConsolePanel() = default;

    ConsolePanel::~ConsolePanel()
    {
        UnregisterCallbacks();
    }

    void ConsolePanel::RegisterCallbacks()
    {
        Log::SetCoreMessageCallback([this](const std::string& message) { PushMessage(message); });
        Log::SetClientMessageCallback([this](const std::string& message) { PushMessage(message); });
    }

    void ConsolePanel::UnregisterCallbacks()
    {
        Log::SetCoreMessageCallback(nullptr);
        Log::SetClientMessageCallback(nullptr);
    }

    void ConsolePanel::Draw()
    {
        ImGui::Begin("Console");

        if (ImGui::Button("Clear"))
        {
            std::scoped_lock lock(m_MessagesMutex);
            m_Messages.clear();
        }

        ImGui::Separator();
        ImGui::BeginChild("ConsoleScrollingRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

        {
            std::scoped_lock lock(m_MessagesMutex);
            for (const std::string& message : m_Messages)
            {
                ImGui::TextUnformatted(message.c_str());
            }
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void ConsolePanel::PushMessage(const std::string& message)
    {
        std::scoped_lock lock(m_MessagesMutex);

        constexpr size_t MaxMessages = 500;
        m_Messages.push_back(message);
        if (m_Messages.size() > MaxMessages)
        {
            m_Messages.erase(m_Messages.begin());
        }
    }
}
