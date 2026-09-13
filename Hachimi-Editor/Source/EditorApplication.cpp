#include "EditorApplication.h"

#include "Core/EntryPoint.h"
#include "Core/Log.h"
#include "Panels/ProjectHubLayer.h"

#include <filesystem>
#include <string>

namespace HachimiEngine
{
    namespace
    {
        // "--project <path.hproj>" opens straight into a project instead of the hub. The editor is
        // a console application, so a bad argument is reported on the console and the hub opens.
        std::filesystem::path ParseProjectArgument(int argc, char** argv)
        {
            for (int index = 1; index < argc; ++index)
            {
                const std::string argument(argv[index]);
                if (argument == "--project" && index + 1 < argc)
                {
                    return std::filesystem::path(argv[index + 1]);
                }
                if (argument.starts_with("--project="))
                {
                    return std::filesystem::path(argument.substr(std::string("--project=").size()));
                }
            }
            return {};
        }
    }

    EditorApplication::EditorApplication(const std::filesystem::path& projectFile)
        : Application(WindowProps("Hachimi-Editor", 1600, 900))
    {
        HE_CLIENT_INFO("Hachimi-Editor started");
        PushLayer(CreateRef<ProjectHubLayer>(projectFile));
    }
}

namespace HachimiEngine
{
    Application* CreateApplication(int argc, char** argv)
    {
        const std::filesystem::path projectFile = ParseProjectArgument(argc, argv);
        return new EditorApplication(projectFile);
    }
}
