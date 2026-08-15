#pragma once

#include "Core/Base.h"

#include <functional>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace HachimiEngine
{
    // Owns the two console loggers used by the engine and the client.
    class Log
    {
    public:
        using ClientMessageCallback = std::function<void(const std::string& message)>;

        static void Init();
        static void Shutdown();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

        // Editor console panels can register one callback per logger category.
        static void SetCoreMessageCallback(ClientMessageCallback callback);
        static void SetClientMessageCallback(ClientMessageCallback callback);

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

// Core (engine) logging macros.
#define HE_CORE_TRACE(...)    ::HE::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define HE_CORE_INFO(...)     ::HE::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HE_CORE_WARN(...)     ::HE::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HE_CORE_ERROR(...)    ::HE::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HE_CORE_CRITICAL(...) ::HE::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client (editor / application) logging macros.
#define HE_CLIENT_TRACE(...)    ::HE::Log::GetClientLogger()->trace(__VA_ARGS__)
#define HE_CLIENT_INFO(...)     ::HE::Log::GetClientLogger()->info(__VA_ARGS__)
#define HE_CLIENT_WARN(...)     ::HE::Log::GetClientLogger()->warn(__VA_ARGS__)
#define HE_CLIENT_ERROR(...)    ::HE::Log::GetClientLogger()->error(__VA_ARGS__)
#define HE_CLIENT_CRITICAL(...) ::HE::Log::GetClientLogger()->critical(__VA_ARGS__)
