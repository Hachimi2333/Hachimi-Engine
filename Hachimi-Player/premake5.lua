-- Hachimi-Player standalone game runtime executable.

project "Hachimi-Player"
    kind "ConsoleApp"
    language "C++"

    includedirs
    {
        "%{prj.location}/Source",
        "%{wks.location}/Hachimi-Engine/Source",
        "%{wks.location}/Hachimi-Engine/Vendor/EnTT/src",
        "%{wks.location}/Hachimi-Engine/Vendor/EnTT/single_include",
        "%{wks.location}/Hachimi-Engine/Vendor/glm",
        "%{wks.location}/Hachimi-Engine/Vendor/GLAD/include",
        "%{wks.location}/Hachimi-Engine/Vendor/GLFW/include",
        "%{wks.location}/Hachimi-Engine/Vendor/imgui/src",
        "%{wks.location}/Hachimi-Engine/Vendor/spdlog/include",
        "%{wks.location}/Hachimi-Engine/Vendor/yaml-cpp/include",
        "%{wks.location}/Hachimi-Engine/Vendor/stb/src"
    }

    files
    {
        "%{prj.location}/Source/**.h",
        "%{prj.location}/Source/**.cpp"
    }

    defines
    {
        "SPDLOG_COMPILED_LIB",
        "YAML_CPP_STATIC_DEFINE",
        "GLFW_INCLUDE_NONE",
        "GLM_ENABLE_EXPERIMENTAL"
    }

    links
    {
        "Hachimi-Engine",
        "zstd",
        "ImGui",
        "spdlog",
        "yaml-cpp",
        "Box3D",
        "Lua",
        "GLAD",
        "GLFW",
        "opengl32",
        "Ole32",
        "Shell32",
        "Uuid"
    }

    postbuildcommands
    {
        "{MKDIR} \"%{cfg.targetdir}/Assets/Fonts\"",
        "copy /Y \"%{wks.location}Hachimi-Editor\\Assets\\Fonts\\Inter-Regular.ttf\" \"%{cfg.targetdir}/Assets/Fonts/Inter-Regular.ttf\"",
        "copy /Y \"%{wks.location}Hachimi-Editor\\Assets\\Fonts\\OFL.txt\" \"%{cfg.targetdir}/Assets/Fonts/OFL.txt\"",
        "{MKDIR} \"%{cfg.targetdir}/Shaders\"",
        "copy /Y \"%{wks.location}Hachimi-Engine\\Resources\\Shaders\\*.glsl\" \"%{cfg.targetdir}/Shaders/\""
    }
