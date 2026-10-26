#pragma once

#include "Core/Base.h"
#include "Core/Memory.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace HachimiEngine
{
    // What a world's steps did with the engine's worker pool. Reported per world, so a test or an
    // editor panel can tell "this world really forked work onto the pool" from "this world ran on
    // one thread", which is not visible from the outside otherwise.
    struct PhysicsTaskStats
    {
        // Workers Box3D is allowed to use. 1 means every task runs on the stepping thread.
        uint32_t WorkerCount = 1;
        // Whether tasks were handed to the JobSystem pool rather than run where they were enqueued.
        bool UsesThreadPool = false;
        // Tasks the pool ran, and tasks the enqueueing thread ran itself.
        size_t PoolTaskCount = 0;
        size_t InlineTaskCount = 0;
        // Steps refused because they were asked for from a JobSystem worker thread; see TryBeginStep.
        size_t RefusedStepCount = 0;
    };

    // Runs Box3D's step sub-tasks on the JobSystem, and keeps the one rule that makes that safe.
    //
    // Box3D forks work from inside b3World_Step and blocks on every task it spawned before the step
    // continues - that is what b3FinishTaskCallback is for (box3d/types.h). The stepping thread is
    // therefore a thread that must be allowed to block, which is the whole reason for this class's
    // iron rule:
    //
    //   b3World_Step runs on the thread that owns the frame, never inside a JobSystem job.
    //
    // A job that parks on its own sub-tasks can hold the last worker the pool has, and then nothing
    // is left to run them. Box3D says so in its own header. Stepping stays in PhysicsSystem::OnUpdate
    // (the main thread), no API here steps a world, and TryBeginStep refuses a step asked for from a
    // pool worker instead of hanging on one.
    //
    // The second half of the rule is about the pool being optional: with a worker count of 1 there is
    // nothing to fork, so every task runs inline in Enqueue and Enqueue answers "already done" by
    // returning nullptr - exactly the contract for a task the enqueueing callback ran itself. A world
    // created before JobSystem::Init(), or configured with WorkerCount = 1, simulates single-threaded
    // and behaves as it did before threads existed.
    //
    // This class deliberately knows nothing about Box3D: the backend's task shape is a function
    // pointer plus an opaque context, which is all the callbacks in PhysicsWorld need to forward.
    class PhysicsTaskScheduler
    {
    public:
        // Box3D's own limit on workers per world (B3_MAX_WORKERS); a larger request is clamped to it.
        // PhysicsWorld static-asserts the two stay in step.
        static constexpr uint32_t MaxWorkers = 32;

        // A Box3D task and the context Box3D wants it called with.
        using TaskFunction = void (*)(void* context);

        // requestedWorkerCount 0 picks the JobSystem's worker count; any request that cannot actually
        // run in parallel - a pool that is not running, or a request of 1 - resolves to 1.
        explicit PhysicsTaskScheduler(uint32_t requestedWorkerCount);
        // Out of line because the slot list owns incomplete types until the source file completes it.
        ~PhysicsTaskScheduler();

        // Re-resolves a requested count and returns the count Box3D must be told. Meant to be called
        // once per step: resolution reads the JobSystem's state, so a world created before
        // JobSystem::Init() picks the pool up as soon as the settings ask for it.
        uint32_t SetWorkerCount(uint32_t requestedWorkerCount);

        // The count the last resolution produced. 1 while the world runs single-threaded.
        uint32_t GetWorkerCount() const { return m_WorkerCount; }

        // Whether tasks go to the pool. False in single-threaded mode, which is also what makes
        // Enqueue's return value always nullptr.
        bool UsesThreadPool() const { return m_UsesThreadPool; }

        // Called once before a step. Returns false - and logs the first time - when the calling thread
        // is a JobSystem worker, because stepping there is the deadlock described above. The step is
        // refused rather than attempted: a hung process is worse than a frame that did not simulate.
        bool TryBeginStep();

        // Hands a task to the pool, or runs it on the calling thread. Returns the token Finish needs,
        // or nullptr when the task already ran to completion.
        void* Enqueue(TaskFunction task, void* taskContext);

        // Blocks until the task Enqueue handed out completed, then recycles its slot. A nullptr token
        // is a no-op, so a caller can pass through whatever Enqueue returned.
        void Finish(void* userTask);

        PhysicsTaskStats GetStats() const;

        // The worker count a request resolves to, given the pool's state. Pure, so the fallback rules
        // can be tested without a pool that is stopped - which in this engine only happens at exit.
        static uint32_t ResolveWorkerCount(uint32_t requestedWorkerCount, bool jobSystemRunning, uint32_t jobSystemWorkers);

    private:
        // One in-flight task: the handle its Finish waits on. Recycled through a free list, so a step
        // allocates nothing once the high-water mark of its task count has been seen.
        struct Slot;

        Slot* AcquireSlot();
        void ReleaseSlot(Slot* slot);

        std::vector<Scope<Slot>> m_Slots;
        std::vector<Slot*> m_FreeSlots;
        uint32_t m_WorkerCount = 1;
        bool m_UsesThreadPool = false;
        bool m_ReportedRefusedStep = false;
        size_t m_PoolTaskCount = 0;
        size_t m_InlineTaskCount = 0;
        size_t m_RefusedStepCount = 0;
    };
}
