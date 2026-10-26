#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace HachimiEngine
{
    // Reports on one job submitted with JobSystem::SubmitTracked.
    //
    // The pool owns the work and the handle only observes it, so dropping a handle neither cancels
    // the job nor waits for it, and the job outlives every handle that watched it. Moving is fine -
    // a handle is a claim on one job's completion, not the job itself - but copying is not, so that
    // waiting on a job always has exactly one owner to ask.
    class JobHandle
    {
    public:
        // Stands for work that never existed, and therefore reads as already complete. That keeps
        // Wait() on an empty handle a no-op instead of a special case at every call site.
        JobHandle() = default;
        ~JobHandle() = default;

        JobHandle(JobHandle&& other) noexcept = default;
        JobHandle& operator=(JobHandle&& other) noexcept = default;

        // One job has one completion; two handles to it would make "the" wait ambiguous.
        JobHandle(const JobHandle&) = delete;
        JobHandle& operator=(const JobHandle&) = delete;

        bool IsComplete() const;

        // Blocks until the job completed. Never call this from a JobSystem worker thread: the pool
        // is the only thing that can run the job being waited for, so a worker that parks here can
        // starve it - the deadlock Box3D warns about for a step running inside a job. IsWorkerThread()
        // answers whether the calling thread is a worker.
        void Wait() const;

    private:
        friend class JobSystem;

        struct Completion
        {
            std::atomic<bool> Finished { false };
        };

        explicit JobHandle(Ref<Completion> completion)
            : m_Completion(std::move(completion))
        {
        }

        Ref<Completion> m_Completion;
    };

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

        // Submits a job and returns a handle that reports when it finished. For callers that must
        // know a specific job is done rather than merely submitted - Box3D blocks on every sub-task
        // it spawns, so it needs one handle per task. Like Submit, a job handed in while the pool is
        // not running executes inline and the returned handle is already complete.
        static JobHandle SubmitTracked(std::function<void()> job);

        // Whether the calling thread is one of the pool's workers. Code that would block on another
        // job checks this before waiting, because a parked worker cannot run what it waits for.
        static bool IsWorkerThread();

        static uint32_t GetWorkerCount();
        static size_t GetPendingJobCount();
    };
}
