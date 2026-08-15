#include "Core/Log.h"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>

namespace HachimiEngine
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    namespace
    {
        struct CallbackState
        {
            std::mutex Mutex;
            Log::ClientMessageCallback Callback;
        };

        std::shared_ptr<CallbackState> s_CoreCallbackState;
        std::shared_ptr<CallbackState> s_ClientCallbackState;

        std::shared_ptr<spdlog::sinks::callback_sink_mt> MakeMessageSink(const std::shared_ptr<CallbackState>& state)
        {
            return std::make_shared<spdlog::sinks::callback_sink_mt>([state](const spdlog::details::log_msg& message)
            {
                std::scoped_lock lock(state->Mutex);
                if (state->Callback)
                {
                    state->Callback({ message.payload.data(), message.payload.size() });
                }
            });
        }
    }

    void Log::Init()
    {
        spdlog::set_pattern("%^[%T.%e] [%n] [%l] %v%$");

        s_CoreCallbackState = std::make_shared<CallbackState>();
        s_ClientCallbackState = std::make_shared<CallbackState>();

        auto coreConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto clientConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        s_CoreLogger = std::make_shared<spdlog::logger>("ENGINE", coreConsoleSink);
        s_ClientLogger = std::make_shared<spdlog::logger>("CLIENT", clientConsoleSink);

        s_CoreLogger->sinks().push_back(MakeMessageSink(s_CoreCallbackState));
        s_ClientLogger->sinks().push_back(MakeMessageSink(s_ClientCallbackState));

        s_CoreLogger->set_level(spdlog::level::trace);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
    }

    void Log::Shutdown()
    {
        s_CoreLogger.reset();
        s_ClientLogger.reset();
        s_CoreCallbackState.reset();
        s_ClientCallbackState.reset();
        spdlog::shutdown();
    }

    void Log::SetCoreMessageCallback(ClientMessageCallback callback)
    {
        if (s_CoreCallbackState)
        {
            std::scoped_lock lock(s_CoreCallbackState->Mutex);
            s_CoreCallbackState->Callback = std::move(callback);
        }
    }

    void Log::SetClientMessageCallback(ClientMessageCallback callback)
    {
        if (s_ClientCallbackState)
        {
            std::scoped_lock lock(s_ClientCallbackState->Mutex);
            s_ClientCallbackState->Callback = std::move(callback);
        }
    }
}
