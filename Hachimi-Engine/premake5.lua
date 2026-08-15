-- Hachimi-Engine core static library project.

project "Hachimi-Engine"
    kind "StaticLib"
    language "C++"

    includedirs
    {
        "%{prj.location}/Source",
        "%{prj.location}/Vendor/EnTT/src",
        "%{prj.location}/Vendor/EnTT/single_include",
        "%{prj.location}/Vendor/glm",
        "%{prj.location}/Vendor/GLAD/include",
        "%{prj.location}/Vendor/GLFW/include",
        "%{prj.location}/Vendor/imgui/src",
        "%{prj.location}/Vendor/spdlog/include",
        "%{prj.location}/Vendor/yaml-cpp/include",
        "%{prj.location}/Vendor/stb/src"
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
        "GLFW",
        "GLAD",
        "ImGui",
        "spdlog",
        "yaml-cpp",
        "opengl32"
    }
