#include "Core/Log.h"
#include "Core/StartupTimer.h"
#include "Renderer/EnvironmentMapData.h"

#include <filesystem>
#include <fstream>
#include <exception>

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    HE::Log::Init();
    int exitCode = 0;
    try
    {
        HE::StartupTimer timer("Environment bake");
        const auto bytes = HE::EnvironmentMapData::Generate().Serialize();
        const std::filesystem::path path(argv[1]);
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (!file) throw std::runtime_error("Could not write baked environment");
    }
    catch (const std::exception& error)
    {
        HE_CORE_ERROR("{}", error.what());
        exitCode = 1;
    }
    HE::Log::Shutdown();
    return exitCode;
}
