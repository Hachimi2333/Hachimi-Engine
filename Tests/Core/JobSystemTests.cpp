// JobSystem: the engine's worker pool.
//
// main() owns the system's lifetime, so no case here calls Shutdown(): the async read
// suite still needs the pool afterwards.

#include <doctest/doctest.h>

#include "Core/JobSystem.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace HachimiEngine;

namespace
{
    size_t CountHits(const std::vector<std::atomic<int>>& hits)
    {
        size_t total = 0;
        for (const std::atomic<int>& hit : hits)
        {
            total += static_cast<size_t>(hit.load());
        }
        return total;
    }
}

TEST_SUITE_BEGIN("Core");

TEST_CASE("submitted jobs all run exactly once")
{
    REQUIRE(JobSystem::IsRunning());
    CHECK(JobSystem::GetWorkerCount() >= 1);

    constexpr size_t jobCount = 256;
    std::vector<std::atomic<int>> hits(jobCount);
    for (std::atomic<int>& hit : hits)
    {
        hit.store(0);
    }

    for (size_t index = 0; index < jobCount; ++index)
    {
        JobSystem::Submit([&hits, index]() { hits[index].fetch_add(1); });
    }

    // GetPendingJobCount() only counts queued work, so the hits are what tell us the pool
    // is done rather than merely drained.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && CountHits(hits) < jobCount)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const size_t total = CountHits(hits);
    INFO("jobs completed: ", total, " of ", jobCount);
    REQUIRE(total == jobCount);

    size_t ranTwice = 0;
    for (const std::atomic<int>& hit : hits)
    {
        if (hit.load() != 1)
        {
            ++ranTwice;
        }
    }

    CHECK(ranTwice == 0);
    CHECK(JobSystem::GetPendingJobCount() == 0);
}

TEST_CASE("jobs run in parallel across workers")
{
    REQUIRE(JobSystem::IsRunning());

    constexpr size_t jobCount = 32;
    std::atomic<int> running { 0 };
    std::atomic<int> peak { 0 };
    std::atomic<int> completed { 0 };

    for (size_t index = 0; index < jobCount; ++index)
    {
        JobSystem::Submit([&running, &peak, &completed]()
        {
            const int concurrent = running.fetch_add(1) + 1;
            int observed = peak.load();
            while (observed < concurrent && !peak.compare_exchange_weak(observed, concurrent))
            {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            running.fetch_sub(1);
            completed.fetch_add(1);
        });
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline && completed.load() < static_cast<int>(jobCount))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    INFO("peak concurrent jobs: ", peak.load(), " of ", JobSystem::GetWorkerCount(), " worker(s)");
    REQUIRE(completed.load() == static_cast<int>(jobCount));

    // One worker can only ever run one job at a time; more workers must overlap.
    if (JobSystem::GetWorkerCount() > 1)
    {
        CHECK(peak.load() > 1);
    }
    else
    {
        CHECK(peak.load() == 1);
    }
}

TEST_SUITE_END();
