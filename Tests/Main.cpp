// Tests: doctest entry point.
//
// This is the only translation unit that defines DOCTEST_CONFIG_IMPLEMENT, so the doctest
// implementation is compiled exactly once, into this executable. It also owns the engine
// lifetime: the log and the job system are up before the first test case runs and are shut
// down after the last one.
//
// Run from a VS 2026 Developer Command Prompt:
//
//   Build/x64-debug/bin/Tests.exe                        # everything
//   Build/x64-debug/bin/Tests.exe -ts=Packaging          # one suite
//   Build/x64-debug/bin/Tests.exe -tc="*round trip*"     # matching cases
//   Build/x64-debug/bin/Tests.exe --list-test-cases      # case names
//
// Everything here runs headless: no window and no OpenGL context is ever created.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "Core/JobSystem.h"
#include "Core/Log.h"
#include "Support/TestWorkspace.h"
#include "Utils/VirtualFileSystem.h"

int main(int argc, char** argv)
{
    HE::Log::Init();
    HE::JobSystem::Init();
    HE::VirtualFileSystem::ResetStatistics();

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();

    // The workspace holds packages and sample files that the engine opened; tear it down
    // before the systems it was built with.
    HE::Tests::DestroyWorkspace();
    HE::VirtualFileSystem::UnmountAll();
    HE::JobSystem::Shutdown();
    HE::Log::Shutdown();

    return result;
}
