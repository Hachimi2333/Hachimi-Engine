#pragma once

#include "Core/Application.h"
#include "Core/Assert.h"
#include "Core/Log.h"

// Every client application defines CreateApplication(); EntryPoint provides main().
namespace HachimiEngine
{
    extern Application* CreateApplication(int argc, char** argv);
}

int main(int argc, char** argv)
{
    HE::Log::Init();

    HE::Application* application = HE::CreateApplication(argc, argv);
    HE_CORE_ASSERT(application != nullptr);

    application->Run();
    delete application;

    HE::Log::Shutdown();
    return 0;
}
