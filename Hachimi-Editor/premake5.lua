-- Hachimi-Editor client executable project.

project "Hachimi-Editor"
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
        "%{wks.location}/Hachimi-Engine/Vendor/stb/src",
        "%{wks.location}/Hachimi-Editor/Vendor/ImGuiFileDialog/src",
        "%{wks.location}/Hachimi-Editor/Vendor/ImGuizmo/src"
    }

    files
    {
        "%{prj.location}/Source/**.h",
        "%{prj.location}/Source/**.cpp",
        "%{prj.location}/Assets/Fonts/Inter-Regular.ttf",
        "%{prj.location}/Assets/Fonts/OFL.txt"
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
        "ImGuiFileDialog",
        "ImGuizmo",
        "ImGui",
        "spdlog",
        "yaml-cpp",
        "Box3D",
        "GLAD",
        "GLFW",
        "opengl32"
    }

    postbuildcommands
    {
        "{MKDIR} \"%{cfg.targetdir}/Assets/Fonts\"",
        "copy /Y \"%{prj.location}Assets\\Fonts\\Inter-Regular.ttf\" \"%{cfg.targetdir}/Assets/Fonts/Inter-Regular.ttf\"",
        "copy /Y \"%{prj.location}Assets\\Fonts\\OFL.txt\" \"%{cfg.targetdir}/Assets/Fonts/OFL.txt\""
    }
