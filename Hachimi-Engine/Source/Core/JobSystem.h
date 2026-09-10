#pragma once

#include "Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace HachimiEngine
{
    // Small engine-owned worker pool. Jobs run on background threads and must not
    // touch GPU resources; uploading decoded data stays on the main thread.
    class JobSystem
    {
    public:
        // workerCount 0 selects hardware_concurrency() - 1, clamped to at least 1.
        static void Init(uint32_t workerCount = 0);
        // Cancels jobs that have not started yet, drains running ones and joins
        // every worker. Safe to call when the system is not running.
        static void Shutdown();

        static bool IsRunning();
        static void Submit(std::function<void()> job);

        static uint32_t GetWorkerCount();
        static size_t GetPendingJobCount();
    };
}
