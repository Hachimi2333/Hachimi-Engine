// VirtualFileSystem asynchronous reads: delivery, cancellation and teardown.

#include <doctest/doctest.h>

#include "Support/TestWorkspace.h"
#include "Utils/VirtualFileSystem.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace HachimiEngine;
using namespace HachimiEngine::Tests;

namespace
{
    // The callback is dispatched from PumpCompletedRequests(), so it may run on the main
    // thread or on a worker; either way it only records, and every assertion below stays
    // on the test thread.
    void PumpFor(const int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; ++elapsed)
        {
            VirtualFileSystem::PumpCompletedRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

TEST_SUITE_BEGIN("Utils");

TEST_CASE("an asynchronous read delivers its bytes to the callback")
{
    const TestWorkspace& workspace = Workspace();
    const Sample* sample = workspace.FindSample(MultiBlockEntry);
    REQUIRE(sample != nullptr);

    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());

    int callbackCount = 0;
    bool asyncSuccess = false;
    std::vector<uint8_t> asyncBytes;

    const uint64_t requestId = VirtualFileSystem::ReadFileAsync(mount.MountPoint() / MultiBlockEntry,
        [&](bool success, std::vector<uint8_t>&& data)
        {
            ++callbackCount;
            asyncSuccess = success;
            asyncBytes = std::move(data);
        });

    REQUIRE(requestId != 0);

    for (int spin = 0; spin < 2000 && callbackCount == 0; ++spin)
    {
        VirtualFileSystem::PumpCompletedRequests();
        if (callbackCount == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    CHECK(callbackCount == 1);
    CHECK(asyncSuccess);
    CHECK(SameBytes(asyncBytes, sample->Content));
    CHECK(VirtualFileSystem::GetPendingRequestCount() == 0);
}

TEST_CASE("a cancelled request never invokes its callback")
{
    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());

    int callbacks = 0;
    const uint64_t requestId = VirtualFileSystem::ReadFileAsync(mount.MountPoint() / MultiBlockEntry,
        [&](bool, std::vector<uint8_t>&&) { ++callbacks; });
    REQUIRE(requestId != 0);

    CHECK(VirtualFileSystem::IsRequestPending(requestId));
    VirtualFileSystem::CancelRequest(requestId);
    PumpFor(400);

    CHECK(callbacks == 0);
    CHECK(VirtualFileSystem::GetPendingRequestCount() == 0);
}

TEST_CASE("unmounting drops outstanding requests without invoking their callbacks")
{
    ScopedArchiveMount mount;
    REQUIRE(mount.IsMounted());

    int callbacks = 0;
    const uint64_t requestId = VirtualFileSystem::ReadFileAsync(mount.MountPoint() / MultiBlockEntry,
        [&](bool, std::vector<uint8_t>&&) { ++callbacks; });
    REQUIRE(requestId != 0);

    VirtualFileSystem::UnmountAll();
    PumpFor(200);

    CHECK(callbacks == 0);
    CHECK(VirtualFileSystem::GetPendingRequestCount() == 0);
}

TEST_SUITE_END();
