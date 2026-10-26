#pragma once

#include "Core/Application.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/StartupTimer.h"
#include "Core/JobSystem.h"
#include "Utils/VirtualFileSystem.h"

#include <cstring>
#include <exception>

// Every client application defines CreateApplication(); EntryPoint provides main().
namespace HachimiEngine
{
    extern Application* CreateApplication(int argc, char** argv);
}

int main(int argc, char** argv)
{
    HE::Log::Init();
    HE::StartupTimer::Begin();

    // Shared diagnostic option: submit one real frame and exit without UI automation.
    bool startupProfile = false;
    for (int index = 1; index < argc;)
    {
        if (std::strcmp(argv[index], "--startup-profile") != 0) { ++index; continue; }
        startupProfile = true;
        for (int next = index; next < argc; ++next) argv[next] = argv[next + 1];
        --argc;
    }

    int exitCode = 0;
    try
    {
        HE::Scope<HE::Application> application(HE::CreateApplication(argc, argv));
        HE_CORE_ASSERT(application != nullptr);
        application->Run(startupProfile);
    }
    catch (const std::exception& error)
    {
        HE_CORE_ERROR("Startup failed: {}", error.what());
        HE::JobSystem::Shutdown();
        HE::VirtualFileSystem::UnmountAll();
        exitCode = 1;
    }

    HE::Log::Shutdown();
    return exitCode;
}
