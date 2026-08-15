#include "EditorApplication.h"

#include "Core/EntryPoint.h"
#include "Core/Log.h"

namespace HachimiEngine
{
    EditorApplication::EditorApplication()
        : Application(WindowProps("Hachimi-Editor", 1600, 900))
    {
        HE_CLIENT_INFO("Hachimi-Editor started");
    }
}

namespace HachimiEngine
{
    Application* CreateApplication(int argc, char** argv)
    {
        return new EditorApplication();
    }
}
