#include "Physics/PhysicsTaskScheduler.h"

#include "Core/JobSystem.h"
#include "Core/Log.h"

#include <algorithm>

namespace HachimiEngine
{
    struct PhysicsTaskScheduler::Slot
    {
        JobHandle Handle;
    };

    PhysicsTaskScheduler::PhysicsTaskScheduler(uint32_t requestedWorkerCount)
    {
        SetWorkerCount(requestedWorkerCount);
    }

    // Box3D has finished every task it enqueued by the time a step returns, so destroying a scheduler
    // never interrupts work in flight; the slot list simply drops the recycled handles.
    PhysicsTaskScheduler::~PhysicsTaskScheduler() = default;

    uint32_t PhysicsTaskScheduler::ResolveWorkerCount(
        uint32_t requestedWorkerCount,
        bool jobSystemRunning,
        uint32_t jobSystemWorkers)
    {
        if (requestedWorkerCount == 1)
        {
            return 1;
        }

        // Only a running pool can run a task somewhere else. Reporting workers Box3D cannot actually
        // get would make it fork tasks that this scheduler then has to run inline, and some of those
        // (the solver's worker tasks) spin waiting for the task that orchestrates them - which only
        // runs once the enqueueing thread is done enqueueing. A pool that is not running therefore
        // reports the single worker that Box3D runs synchronously, which is also the count its own
        // serial fallback uses.
        if (!jobSystemRunning)
        {
            return 1;
        }

        const uint32_t available = std::max(jobSystemWorkers, 1u);
        const uint32_t resolved = requestedWorkerCount == 0 ? available : requestedWorkerCount;
        return std::clamp(resolved, 1u, std::min(available, MaxWorkers));
    }

    uint32_t PhysicsTaskScheduler::SetWorkerCount(uint32_t requestedWorkerCount)
    {
        const bool poolRunning = JobSystem::IsRunning();
        m_WorkerCount = ResolveWorkerCount(requestedWorkerCount, poolRunning, JobSystem::GetWorkerCount());

        // One worker is not worth a round trip through the pool: Box3D forks nothing with a single
        // worker, and running that one task inline keeps the step on its own thread end to end.
        m_UsesThreadPool = poolRunning && m_WorkerCount > 1;
        return m_WorkerCount;
    }

    bool PhysicsTaskScheduler::TryBeginStep()
    {
        if (!JobSystem::IsWorkerThread())
        {
            return true;
        }

        ++m_RefusedStepCount;
        if (!m_ReportedRefusedStep)
        {
            m_ReportedRefusedStep = true;
            HE_CORE_ERROR(
                "Refused a physics step requested inside a JobSystem job: b3World_Step parks its own "
                "thread on the tasks it spawns, so a pooled job that steps a world can deadlock the "
                "pool it needs. Step from the frame's thread (PhysicsSystem::OnUpdate). Further "
                "refusals are counted in PhysicsTaskStats::RefusedStepCount.");
        }

        return false;
    }

    void* PhysicsTaskScheduler::Enqueue(TaskFunction task, void* taskContext)
    {
        if (task == nullptr)
        {
            return nullptr;
        }

        if (!m_UsesThreadPool)
        {
            // Box3D's serial contract: the enqueueing callback ran the task itself, so there is
            // nothing to finish and the return value says so.
            //
            // This is only ever reached with a worker count of 1, and that is what makes it safe: a
            // single-worker Box3D forks one task per fork point and that task is the one driving the
            // rest of the work, so running it here completes it. The solver's helper tasks are the
            // exception - they spin until the task that orchestrates them starts - which is exactly
            // why a request for workers this machine cannot provide resolves to 1 instead of being
            // passed on and then run inline.
            task(taskContext);
            ++m_InlineTaskCount;
            return nullptr;
        }

        Slot* slot = AcquireSlot();
        slot->Handle = JobSystem::SubmitTracked([task, taskContext]() { task(taskContext); });
        ++m_PoolTaskCount;
        return slot;
    }

    void PhysicsTaskScheduler::Finish(void* userTask)
    {
        if (userTask == nullptr)
        {
            return;
        }

        Slot* slot = static_cast<Slot*>(userTask);
        // Box3D blocks its own thread here, which is why TryBeginStep keeps steps off pool workers.
        slot->Handle.Wait();
        ReleaseSlot(slot);
    }

    PhysicsTaskStats PhysicsTaskScheduler::GetStats() const
    {
        return PhysicsTaskStats {
            .WorkerCount = m_WorkerCount,
            .UsesThreadPool = m_UsesThreadPool,
            .PoolTaskCount = m_PoolTaskCount,
            .InlineTaskCount = m_InlineTaskCount,
            .RefusedStepCount = m_RefusedStepCount,
        };
    }

    PhysicsTaskScheduler::Slot* PhysicsTaskScheduler::AcquireSlot()
    {
        if (!m_FreeSlots.empty())
        {
            Slot* slot = m_FreeSlots.back();
            m_FreeSlots.pop_back();
            return slot;
        }

        // The Slot outlives its unique_ptr's moves: the pointer handed to Box3D addresses the object
        // on the heap, not the vector element that owns it.
        m_Slots.push_back(CreateScope<Slot>());
        return m_Slots.back().get();
    }

    void PhysicsTaskScheduler::ReleaseSlot(Slot* slot)
    {
        // Drop the handle so a recycled slot does not keep a finished job's completion state alive.
        slot->Handle = JobHandle();
        m_FreeSlots.push_back(slot);
    }
}
