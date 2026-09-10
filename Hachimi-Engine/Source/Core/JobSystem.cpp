#include "Core/JobSystem.h"

#include "Core/Log.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace HachimiEngine
{
    namespace
    {
        std::vector<std::thread> s_Workers;
        std::deque<std::function<void()>> s_Jobs;
        std::mutex s_QueueMutex;
        std::condition_variable s_QueueCondition;
        bool s_Running = false;
        bool s_AcceptingJobs = false;

        void WorkerLoop()
        {
            for (;;)
            {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(s_QueueMutex);
                    s_QueueCondition.wait(lock, [] { return !s_AcceptingJobs || !s_Jobs.empty(); });

                    // Shutdown sets s_AcceptingJobs to false: drain what is left,
                    // then exit so no callback can fire after teardown.
                    if (!s_AcceptingJobs && s_Jobs.empty())
                    {
                        return;
                    }

                    job = std::move(s_Jobs.front());
                    s_Jobs.pop_front();
                }

                if (job)
                {
                    job();
                }
            }
        }
    }

    void JobSystem::Init(uint32_t workerCount)
    {
        if (s_Running)
        {
            HE_CORE_WARN("Job system is already running");
            return;
        }

        if (workerCount == 0)
        {
            const unsigned int hardwareThreads = std::thread::hardware_concurrency();
            workerCount = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
        }

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_AcceptingJobs = true;
        }

        s_Running = true;
        s_Workers.reserve(workerCount);
        for (uint32_t index = 0; index < workerCount; ++index)
        {
            s_Workers.emplace_back(WorkerLoop);
        }

        HE_CORE_INFO("Job system started with {} worker thread(s)", workerCount);
    }

    void JobSystem::Shutdown()
    {
        if (!s_Running)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_AcceptingJobs = false;
        }
        s_QueueCondition.notify_all();

        for (std::thread& worker : s_Workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        s_Workers.clear();

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_Jobs.clear();
        }

        s_Running = false;
        HE_CORE_INFO("Job system stopped");
    }

    bool JobSystem::IsRunning()
    {
        return s_Running;
    }

    void JobSystem::Submit(std::function<void()> job)
    {
        if (!job)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            if (s_Running && s_AcceptingJobs)
            {
                s_Jobs.push_back(std::move(job));
                s_QueueCondition.notify_one();
                return;
            }
        }

        // Never drop work submitted before Init() or during teardown: run it
        // inline so callers still observe a completed request. Callers that want
        // parallelism check IsRunning() before submitting.
        job();
    }

    uint32_t JobSystem::GetWorkerCount()
    {
        return static_cast<uint32_t>(s_Workers.size());
    }

    size_t JobSystem::GetPendingJobCount()
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        return s_Jobs.size();
    }
}
